// SPDX-License-Identifier: GPL-2.0-only
/*
 * A V4L2 driver for Samsung S5KJN1 cameras.
 * Copyright (C) 2024 Luca Weiss <luca.weiss@fairphone.com>
 *
 * Based on Sony imx412 camera driver
 * Copyright (C) 2021 Intel Corporation
 */
#include <linux/unaligned.h>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>

/* Streaming Mode */
#define S5KJN1_REG_MODE_SELECT	0x0100
#define S5KJN1_MODE_STANDBY	0x00
#define S5KJN1_MODE_STREAMING	0x01

/* Lines per frame */
#define S5KJN1_REG_LPFR		0x0340

/* Chip ID */
#define S5KJN1_REG_ID		0x0000
#define S5KJN1_ID		0x38E1

/* Exposure control */
#define S5KJN1_REG_EXPOSURE_CIT	0x0202
#define S5KJN1_EXPOSURE_MIN	8
#define S5KJN1_EXPOSURE_OFFSET	22
#define S5KJN1_EXPOSURE_STEP	1
#define S5KJN1_EXPOSURE_DEFAULT	0x0648

/* Analog gain control */
#define S5KJN1_REG_AGAIN	0x0204
#define S5KJN1_AGAIN_MIN	0
#define S5KJN1_AGAIN_MAX	978
#define S5KJN1_AGAIN_STEP	1
#define S5KJN1_AGAIN_DEFAULT	0

/* Group hold register */
#define S5KJN1_REG_HOLD		0x0104

/* Input clock rate */
#define S5KJN1_INCLK_RATE	24000000

/* CSI2 HW configuration */
#define S5KJN1_LINK_FREQ	600000000
#define S5KJN1_NUM_DATA_LANES	4

#define S5KJN1_REG_MIN		0x00
#define S5KJN1_REG_MAX		0xffff

/**
 * struct s5kjn1_reg - s5kjn1 sensor register
 * @address: Register address
 * @val: Register value
 */
struct s5kjn1_reg {
	u16 address;
	u16 val;
};

/**
 * struct s5kjn1_reg_list - s5kjn1 sensor register list
 * @num_of_regs: Number of registers in the list
 * @regs: Pointer to register list
 */
struct s5kjn1_reg_list {
	u32 num_of_regs;
	const struct s5kjn1_reg *regs;
};

/**
 * struct s5kjn1_mode - s5kjn1 sensor mode structure
 * @width: Frame width
 * @height: Frame height
 * @code: Format code
 * @hblank: Horizontal blanking in lines
 * @vblank: Vertical blanking in lines
 * @vblank_min: Minimum vertical blanking in lines
 * @vblank_max: Maximum vertical blanking in lines
 * @pclk: Sensor pixel clock
 * @link_freq_idx: Link frequency index
 * @reg_list: Register list for sensor mode
 */
struct s5kjn1_mode {
	u32 width;
	u32 height;
	u32 code;
	u32 hblank;
	u32 vblank;
	u32 vblank_min;
	u32 vblank_max;
	u64 pclk;
	u32 link_freq_idx;
	struct s5kjn1_reg_list reg_list;
};

static const char * const s5kjn1_supply_names[] = {
	"vddio",	/* 1.8V I/O Power */
	"vddd",		/* 1.05V Digital Power */
	"vdda",		/* 2.8V Analog Power */
};

/**
 * struct s5kjn1 - s5kjn1 sensor device structure
 * @dev: Pointer to generic device
 * @client: Pointer to i2c client
 * @sd: V4L2 sub-device
 * @pad: Media pad. Only one pad supported
 * @reset_gpio: Sensor reset gpio
 * @inclk: Sensor input clock
 * @supplies: Regulator supplies
 * @ctrl_handler: V4L2 control handler
 * @link_freq_ctrl: Pointer to link frequency control
 * @pclk_ctrl: Pointer to pixel clock control
 * @hblank_ctrl: Pointer to horizontal blanking control
 * @vblank_ctrl: Pointer to vertical blanking control
 * @exp_ctrl: Pointer to exposure control
 * @again_ctrl: Pointer to analog gain control
 * @vblank: Vertical blanking in lines
 * @cur_mode: Pointer to current selected sensor mode
 * @mutex: Mutex for serializing sensor controls
 */
struct s5kjn1 {
	struct device *dev;
	struct i2c_client *client;
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct gpio_desc *reset_gpio;
	struct clk *inclk;
	struct regulator_bulk_data supplies[ARRAY_SIZE(s5kjn1_supply_names)];
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *link_freq_ctrl;
	struct v4l2_ctrl *pclk_ctrl;
	struct v4l2_ctrl *hblank_ctrl;
	struct v4l2_ctrl *vblank_ctrl;
	struct {
		struct v4l2_ctrl *exp_ctrl;
		struct v4l2_ctrl *again_ctrl;
	};
	u32 vblank;
	const struct s5kjn1_mode *cur_mode;
	struct mutex mutex;
};

static const s64 link_freq[] = {
	S5KJN1_LINK_FREQ,
};

/* Sensor mode registers */
static const struct s5kjn1_reg mode_2040x1536_regs[] = {
	// common registers
	{ 0x6028, 0x4000 },
	{ 0x0000, 0x0002 },
	{ 0x0000, 0x38e1 },
	{ 0x001e, 0x0007 },
	{ 0x6028, 0x4000 },
	{ 0x6010, 0x0001 },
	// WARNING: Delay 5000 us
	{ 0x6226, 0x0001 },
	// WARNING: Delay 10000 us
	{ 0x6028, 0x2400 },
	{ 0x602a, 0x1354 },
	{ 0x6f12, 0x0100 },
	{ 0x6f12, 0x7017 },
	{ 0x602a, 0x13b2 },
	{ 0x6f12, 0x0000 },
	{ 0x602a, 0x1236 },
	{ 0x6f12, 0x0000 },
	{ 0x602a, 0x1a0a },
	{ 0x6f12, 0x4c0a },
	{ 0x602a, 0x2210 },
	{ 0x6f12, 0x3401 },
	{ 0x602a, 0x2176 },
	{ 0x6f12, 0x6400 },
	{ 0x602a, 0x222e },
	{ 0x6f12, 0x0001 },
	{ 0x602a, 0x06b6 },
	{ 0x6f12, 0x0a00 },
	{ 0x602a, 0x06bc },
	{ 0x6f12, 0x1001 },
	{ 0x602a, 0x2140 },
	{ 0x6f12, 0x0101 },
	{ 0x602a, 0x1a0e },
	{ 0x6f12, 0x9600 },
	{ 0x6028, 0x4000 },
	{ 0xf44e, 0x0011 },
	{ 0xf44c, 0x0b0b },
	{ 0xf44a, 0x0006 },
	{ 0x0118, 0x0002 },
	{ 0x011a, 0x0001 },

	// Res 5 MIPI 4-Lane 2040x1536 10-bit 30fps 800Mbps/lane for small Video
	{ 0x6028, 0x2400 },
	{ 0x602a, 0x1a28 },
	{ 0x6f12, 0x4c00 },
	{ 0x602a, 0x065a },
	{ 0x6f12, 0x0000 },
	{ 0x602a, 0x139e },
	{ 0x6f12, 0x0300 },
	{ 0x602a, 0x139c },
	{ 0x6f12, 0x0000 },
	{ 0x602a, 0x13a0 },
	{ 0x6f12, 0x0a00 },
	{ 0x6f12, 0x0020 },
	{ 0x602a, 0x2072 },
	{ 0x6f12, 0x0000 },
	{ 0x602a, 0x1a64 },
	{ 0x6f12, 0x0301 },
	{ 0x6f12, 0x3f00 },
	{ 0x602a, 0x19e6 },
	{ 0x6f12, 0x0201 },
	{ 0x602a, 0x1a30 },
	{ 0x6f12, 0x3401 },
	{ 0x602a, 0x19fc },
	{ 0x6f12, 0x0b00 },
	{ 0x602a, 0x19f4 },
	{ 0x6f12, 0x0606 },
	{ 0x602a, 0x19f8 },
	{ 0x6f12, 0x1010 },
	{ 0x602a, 0x1b26 },
	{ 0x6f12, 0x6f80 },
	{ 0x6f12, 0xa020 },
	{ 0x602a, 0x1a3c },
	{ 0x6f12, 0x5207 },
	{ 0x602a, 0x1a48 },
	{ 0x6f12, 0x5207 },
	{ 0x602a, 0x1444 },
	{ 0x6f12, 0x2100 },
	{ 0x6f12, 0x2100 },
	{ 0x602a, 0x144c },
	{ 0x6f12, 0x4200 },
	{ 0x6f12, 0x4200 },
	{ 0x602a, 0x7f6c },
	{ 0x6f12, 0x0100 },
	{ 0x6f12, 0x3100 },
	{ 0x6f12, 0xf700 },
	{ 0x6f12, 0x2600 },
	{ 0x6f12, 0xe100 },
	{ 0x602a, 0x0650 },
	{ 0x6f12, 0x0600 },
	{ 0x602a, 0x0654 },
	{ 0x6f12, 0x0000 },
	{ 0x602a, 0x1a46 },
	{ 0x6f12, 0x8600 },
	{ 0x602a, 0x1a52 },
	{ 0x6f12, 0xbf00 },
	{ 0x602a, 0x0674 },
	{ 0x6f12, 0x0500 },
	{ 0x6f12, 0x0500 },
	{ 0x6f12, 0x0500 },
	{ 0x6f12, 0x0500 },
	{ 0x602a, 0x0668 },
	{ 0x6f12, 0x0800 },
	{ 0x6f12, 0x0800 },
	{ 0x6f12, 0x0800 },
	{ 0x6f12, 0x0800 },
	{ 0x602a, 0x0684 },
	{ 0x6f12, 0x4001 },
	{ 0x602a, 0x0688 },
	{ 0x6f12, 0x4001 },
	{ 0x602a, 0x147c },
	{ 0x6f12, 0x1000 },
	{ 0x602a, 0x1480 },
	{ 0x6f12, 0x1000 },
	{ 0x602a, 0x19f6 },
	{ 0x6f12, 0x0904 },
	{ 0x602a, 0x0812 },
	{ 0x6f12, 0x0000 },
	{ 0x602a, 0x1a02 },
	{ 0x6f12, 0x0800 },
	{ 0x602a, 0x2148 },
	{ 0x6f12, 0x0100 },
	{ 0x602a, 0x2042 },
	{ 0x6f12, 0x1a00 },
	{ 0x602a, 0x0874 },
	{ 0x6f12, 0x1100 },
	{ 0x602a, 0x09c0 },
	{ 0x6f12, 0x9800 },
	{ 0x602a, 0x09c4 },
	{ 0x6f12, 0x9800 },
	{ 0x602a, 0x19fe },
	{ 0x6f12, 0x0e1c },
	{ 0x602a, 0x4d92 },
	{ 0x6f12, 0x0100 },
	{ 0x602a, 0x84c8 },
	{ 0x6f12, 0x0100 },
	{ 0x602a, 0x4d94 },
	{ 0x6f12, 0x4001 },
	{ 0x6f12, 0x0004 },
	{ 0x6f12, 0x0010 },
	{ 0x6f12, 0x0810 },
	{ 0x6f12, 0x0004 },
	{ 0x6f12, 0x0010 },
	{ 0x6f12, 0x0810 },
	{ 0x6f12, 0x0810 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0010 },
	{ 0x6f12, 0x0010 },
	{ 0x602a, 0x3570 },
	{ 0x6f12, 0x0000 },
	{ 0x602a, 0x3574 },
	{ 0x6f12, 0x9400 },
	{ 0x602a, 0x21e4 },
	{ 0x6f12, 0x0400 },
	{ 0x602a, 0x21ec },
	{ 0x6f12, 0x4f01 },
	{ 0x602a, 0x2080 },
	{ 0x6f12, 0x0100 },
	{ 0x6f12, 0x7f00 },
	{ 0x6f12, 0x0002 },
	{ 0x6f12, 0x8000 },
	{ 0x6f12, 0x0002 },
	{ 0x6f12, 0xc244 },
	{ 0x6f12, 0xd244 },
	{ 0x6f12, 0x14f4 },
	{ 0x6f12, 0x141c },
	{ 0x6f12, 0x111c },
	{ 0x6f12, 0x54f4 },
	{ 0x602a, 0x20ba },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x602a, 0x120e },
	{ 0x6f12, 0x1000 },
	{ 0x602a, 0x212e },
	{ 0x6f12, 0x0a00 },
	{ 0x602a, 0x13ae },
	{ 0x6f12, 0x0102 },
	{ 0x602a, 0x0718 },
	{ 0x6f12, 0x0005 },
	{ 0x602a, 0x0710 },
	{ 0x6f12, 0x0004 },
	{ 0x6f12, 0x0401 },
	{ 0x6f12, 0x0100 },
	{ 0x602a, 0x1b5c },
	{ 0x6f12, 0x0300 },
	{ 0x602a, 0x0786 },
	{ 0x6f12, 0x7701 },
	{ 0x602a, 0x2022 },
	{ 0x6f12, 0x0101 },
	{ 0x6f12, 0x0101 },
	{ 0x602a, 0x1360 },
	{ 0x6f12, 0x0000 },
	{ 0x602a, 0x1376 },
	{ 0x6f12, 0x0200 },
	{ 0x6f12, 0x6038 },
	{ 0x6f12, 0x7038 },
	{ 0x6f12, 0x8038 },
	{ 0x602a, 0x1386 },
	{ 0x6f12, 0x0b00 },
	{ 0x602a, 0x06fa },
	{ 0x6f12, 0x0000 },
	{ 0x602a, 0x4a94 },
	{ 0x6f12, 0x0c00 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0600 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0600 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0c00 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x602a, 0x0a76 },
	{ 0x6f12, 0x1000 },
	{ 0x602a, 0x0aee },
	{ 0x6f12, 0x1000 },
	{ 0x602a, 0x0b66 },
	{ 0x6f12, 0x1000 },
	{ 0x602a, 0x0bde },
	{ 0x6f12, 0x1000 },
	{ 0x602a, 0x0be8 },
	{ 0x6f12, 0x3000 },
	{ 0x6f12, 0x3000 },
	{ 0x602a, 0x0c56 },
	{ 0x6f12, 0x1000 },
	{ 0x602a, 0x0c60 },
	{ 0x6f12, 0x3000 },
	{ 0x6f12, 0x3000 },
	{ 0x602a, 0x0cb6 },
	{ 0x6f12, 0x0000 },
	{ 0x602a, 0x0cf2 },
	{ 0x6f12, 0x0001 },
	{ 0x602a, 0x0cf0 },
	{ 0x6f12, 0x0101 },
	{ 0x602a, 0x11b8 },
	{ 0x6f12, 0x0000 },
	{ 0x602a, 0x11f6 },
	{ 0x6f12, 0x0010 },
	{ 0x602a, 0x4a74 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0xd8ff },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0xd8ff },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x602a, 0x218e },
	{ 0x6f12, 0x0000 },
	{ 0x602a, 0x2268 },
	{ 0x6f12, 0xf279 },
	{ 0x602a, 0x5006 },
	{ 0x6f12, 0x0000 },
	{ 0x602a, 0x500e },
	{ 0x6f12, 0x0100 },
	{ 0x602a, 0x4e70 },
	{ 0x6f12, 0x2062 },
	{ 0x6f12, 0x5501 },
	{ 0x602a, 0x06dc },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x6f12, 0x0000 },
	{ 0x6028, 0x4000 },
	{ 0xf46a, 0xae80 },
	{ 0x0344, 0x0000 },
	{ 0x0346, 0x0000 },
	{ 0x0348, 0x1fff },
	{ 0x034a, 0x181f },
	{ 0x034c, 0x07f8 },
	{ 0x034e, 0x0600 },
	{ 0x0350, 0x0004 },
	{ 0x0352, 0x0004 },
	{ 0x0900, 0x0144 },
	{ 0x0380, 0x0002 },
	{ 0x0382, 0x0006 },
	{ 0x0384, 0x0002 },
	{ 0x0386, 0x0006 },
	{ 0x0110, 0x1002 },
	{ 0x0114, 0x0300 },
	{ 0x0116, 0x3000 },
	{ 0x0136, 0x1800 },
	{ 0x013e, 0x0000 },
	{ 0x0300, 0x0006 },
	{ 0x0302, 0x0001 },
	{ 0x0304, 0x0004 },
	{ 0x0306, 0x0096 },
	{ 0x0308, 0x0008 },
	{ 0x030a, 0x0001 },
	{ 0x030c, 0x0000 },
	{ 0x030e, 0x0003 },
	{ 0x0310, 0x0064 },
	{ 0x0312, 0x0001 },
	{ 0x080e, 0x0000 },
	{ 0x0340, 0x101c },
	{ 0x0342, 0x12f0 },
	{ 0x0702, 0x0000 },
	{ 0x0202, 0x0100 },
	{ 0x0200, 0x0100 },
	{ 0x0d00, 0x0101 },
	{ 0x0d02, 0x0001 },
	{ 0x0d04, 0x0102 },
	{ 0x011e, 0x0100 },
	{ 0x6226, 0x0000 },
};

/* Supported sensor mode configurations */
static const struct s5kjn1_mode supported_mode = {
	.width = 2040,
	.height = 1536,
	.hblank = 456, // FIXME
	.vblank = 506, // FIXME
	.vblank_min = 506, // FIXME
	.vblank_max = 32420, // FIXME
	.pclk = 7920000000, // outputPixelClock?
	.link_freq_idx = 0,
	.code = MEDIA_BUS_FMT_SGRBG10_1X10,
	.reg_list = {
		.num_of_regs = ARRAY_SIZE(mode_2040x1536_regs),
		.regs = mode_2040x1536_regs,
	},
};

/**
 * to_s5kjn1() - s5kjn1 V4L2 sub-device to s5kjn1 device.
 * @subdev: pointer to s5kjn1 V4L2 sub-device
 *
 * Return: pointer to s5kjn1 device
 */
static inline struct s5kjn1 *to_s5kjn1(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct s5kjn1, sd);
}

/**
 * s5kjn1_read_reg() - Read registers.
 * @s5kjn1: pointer to s5kjn1 device
 * @reg: register address
 * @len: length of bytes to read. Max supported bytes is 4
 * @val: pointer to register value to be filled.
 *
 * Return: 0 if successful, error code otherwise.
 */
static int s5kjn1_read_reg(struct s5kjn1 *s5kjn1, u16 reg, u32 len, u32 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&s5kjn1->sd);
	struct i2c_msg msgs[2] = {0};
	u8 addr_buf[2] = {0};
	u8 data_buf[4] = {0};
	int ret;

	if (WARN_ON(len > 4))
		return -EINVAL;

	put_unaligned_be16(reg, addr_buf);

	/* Write register address */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = ARRAY_SIZE(addr_buf);
	msgs[0].buf = addr_buf;

	/* Read data from register */
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = &data_buf[4 - len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	*val = get_unaligned_be32(data_buf);

	return 0;
}

/**
 * s5kjn1_write_reg() - Write register
 * @s5kjn1: pointer to s5kjn1 device
 * @reg: register address
 * @len: length of bytes. Max supported bytes is 4
 * @val: register value
 *
 * Return: 0 if successful, error code otherwise.
 */
static int s5kjn1_write_reg(struct s5kjn1 *s5kjn1, u16 reg, u32 len, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&s5kjn1->sd);
	u8 buf[6] = {0};

	if (WARN_ON(len > 4))
		return -EINVAL;

	put_unaligned_be16(reg, buf);
	put_unaligned_be32(val << (8 * (4 - len)), buf + 2);
	if (i2c_master_send(client, buf, len + 2) != len + 2)
		return -EIO;

	return 0;
}

/**
 * s5kjn1_write_regs() - Write a list of registers
 * @s5kjn1: pointer to s5kjn1 device
 * @regs: list of registers to be written
 * @len: length of registers array
 *
 * Return: 0 if successful, error code otherwise.
 */
static int s5kjn1_write_regs(struct s5kjn1 *s5kjn1,
			     const struct s5kjn1_reg *regs, u32 len)
{
	unsigned int i;
	int ret;

	for (i = 0; i < len; i++) {
		ret = s5kjn1_write_reg(s5kjn1, regs[i].address, 2, regs[i].val);
		if (ret)
			return ret;
	}

	return 0;
}

/**
 * s5kjn1_update_controls() - Update control ranges based on streaming mode
 * @s5kjn1: pointer to s5kjn1 device
 * @mode: pointer to s5kjn1_mode sensor mode
 *
 * Return: 0 if successful, error code otherwise.
 */
static int s5kjn1_update_controls(struct s5kjn1 *s5kjn1,
				  const struct s5kjn1_mode *mode)
{
	int ret;

	ret = __v4l2_ctrl_s_ctrl(s5kjn1->link_freq_ctrl, mode->link_freq_idx);
	if (ret)
		return ret;

	ret = __v4l2_ctrl_s_ctrl(s5kjn1->hblank_ctrl, mode->hblank);
	if (ret)
		return ret;

	return __v4l2_ctrl_modify_range(s5kjn1->vblank_ctrl, mode->vblank_min,
					mode->vblank_max, 1, mode->vblank);
}

/**
 * s5kjn1_update_exp_gain() - Set updated exposure and gain
 * @s5kjn1: pointer to s5kjn1 device
 * @exposure: updated exposure value
 * @gain: updated analog gain value
 *
 * Return: 0 if successful, error code otherwise.
 */
static int s5kjn1_update_exp_gain(struct s5kjn1 *s5kjn1, u32 exposure, u32 gain)
{
	u32 lpfr;
	int ret;

	lpfr = s5kjn1->vblank + s5kjn1->cur_mode->height;

	dev_dbg(s5kjn1->dev, "Set exp %u, analog gain %u, lpfr %u\n",
		exposure, gain, lpfr);

	ret = s5kjn1_write_reg(s5kjn1, S5KJN1_REG_HOLD, 1, 1);
	if (ret)
		return ret;

	ret = s5kjn1_write_reg(s5kjn1, S5KJN1_REG_LPFR, 2, lpfr);
	if (ret)
		goto error_release_group_hold;

	ret = s5kjn1_write_reg(s5kjn1, S5KJN1_REG_EXPOSURE_CIT, 2, exposure);
	if (ret)
		goto error_release_group_hold;

	ret = s5kjn1_write_reg(s5kjn1, S5KJN1_REG_AGAIN, 2, gain);

error_release_group_hold:
	s5kjn1_write_reg(s5kjn1, S5KJN1_REG_HOLD, 1, 0);

	return ret;
}

/**
 * s5kjn1_set_ctrl() - Set subdevice control
 * @ctrl: pointer to v4l2_ctrl structure
 *
 * Supported controls:
 * - V4L2_CID_VBLANK
 * - cluster controls:
 *   - V4L2_CID_ANALOGUE_GAIN
 *   - V4L2_CID_EXPOSURE
 *
 * Return: 0 if successful, error code otherwise.
 */
static int s5kjn1_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct s5kjn1 *s5kjn1 =
		container_of(ctrl->handler, struct s5kjn1, ctrl_handler);
	u32 analog_gain;
	u32 exposure;
	int ret;

	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		s5kjn1->vblank = s5kjn1->vblank_ctrl->val;

		dev_dbg(s5kjn1->dev, "Received vblank %u, new lpfr %u\n",
			s5kjn1->vblank,
			s5kjn1->vblank + s5kjn1->cur_mode->height);

		ret = __v4l2_ctrl_modify_range(s5kjn1->exp_ctrl,
					       S5KJN1_EXPOSURE_MIN,
					       s5kjn1->vblank +
					       s5kjn1->cur_mode->height -
					       S5KJN1_EXPOSURE_OFFSET,
					       1, S5KJN1_EXPOSURE_DEFAULT);
		break;
	case V4L2_CID_EXPOSURE:
		/* Set controls only if sensor is in power on state */
		if (!pm_runtime_get_if_in_use(s5kjn1->dev))
			return 0;

		exposure = ctrl->val;
		analog_gain = s5kjn1->again_ctrl->val;

		dev_dbg(s5kjn1->dev, "Received exp %u, analog gain %u\n",
			exposure, analog_gain);

		ret = s5kjn1_update_exp_gain(s5kjn1, exposure, analog_gain);

		pm_runtime_put(s5kjn1->dev);

		break;
	default:
		dev_err(s5kjn1->dev, "Invalid control %d\n", ctrl->id);
		ret = -EINVAL;
	}

	return ret;
}

/* V4l2 subdevice control ops*/
static const struct v4l2_ctrl_ops s5kjn1_ctrl_ops = {
	.s_ctrl = s5kjn1_set_ctrl,
};

/**
 * s5kjn1_enum_mbus_code() - Enumerate V4L2 sub-device mbus codes
 * @sd: pointer to s5kjn1 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device configuration
 * @code: V4L2 sub-device code enumeration need to be filled
 *
 * Return: 0 if successful, error code otherwise.
 */
static int s5kjn1_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index > 0)
		return -EINVAL;

	code->code = supported_mode.code;

	return 0;
}

/**
 * s5kjn1_enum_frame_size() - Enumerate V4L2 sub-device frame sizes
 * @sd: pointer to s5kjn1 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device configuration
 * @fsize: V4L2 sub-device size enumeration need to be filled
 *
 * Return: 0 if successful, error code otherwise.
 */
static int s5kjn1_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_frame_size_enum *fsize)
{
	if (fsize->index > 0)
		return -EINVAL;

	if (fsize->code != supported_mode.code)
		return -EINVAL;

	fsize->min_width = supported_mode.width;
	fsize->max_width = fsize->min_width;
	fsize->min_height = supported_mode.height;
	fsize->max_height = fsize->min_height;

	return 0;
}

/**
 * s5kjn1_fill_pad_format() - Fill subdevice pad format
 *                            from selected sensor mode
 * @s5kjn1: pointer to s5kjn1 device
 * @mode: pointer to s5kjn1_mode sensor mode
 * @fmt: V4L2 sub-device format need to be filled
 */
static void s5kjn1_fill_pad_format(struct s5kjn1 *s5kjn1,
				   const struct s5kjn1_mode *mode,
				   struct v4l2_subdev_format *fmt)
{
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.code = mode->code;
	fmt->format.field = V4L2_FIELD_NONE;
	fmt->format.colorspace = V4L2_COLORSPACE_RAW;
	fmt->format.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	fmt->format.quantization = V4L2_QUANTIZATION_DEFAULT;
	fmt->format.xfer_func = V4L2_XFER_FUNC_NONE;
}

/**
 * s5kjn1_get_pad_format() - Get subdevice pad format
 * @sd: pointer to s5kjn1 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device configuration
 * @fmt: V4L2 sub-device format need to be set
 *
 * Return: 0 if successful, error code otherwise.
 */
static int s5kjn1_get_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_format *fmt)
{
	struct s5kjn1 *s5kjn1 = to_s5kjn1(sd);

	mutex_lock(&s5kjn1->mutex);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		struct v4l2_mbus_framefmt *framefmt;

		framefmt = v4l2_subdev_state_get_format(sd_state, fmt->pad);
		fmt->format = *framefmt;
	} else {
		s5kjn1_fill_pad_format(s5kjn1, s5kjn1->cur_mode, fmt);
	}

	mutex_unlock(&s5kjn1->mutex);

	return 0;
}

/**
 * s5kjn1_set_pad_format() - Set subdevice pad format
 * @sd: pointer to s5kjn1 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device configuration
 * @fmt: V4L2 sub-device format need to be set
 *
 * Return: 0 if successful, error code otherwise.
 */
static int s5kjn1_set_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_format *fmt)
{
	struct s5kjn1 *s5kjn1 = to_s5kjn1(sd);
	const struct s5kjn1_mode *mode;
	int ret = 0;

	mutex_lock(&s5kjn1->mutex);

	mode = &supported_mode;
	s5kjn1_fill_pad_format(s5kjn1, mode, fmt);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		struct v4l2_mbus_framefmt *framefmt;

		framefmt = v4l2_subdev_state_get_format(sd_state, fmt->pad);
		*framefmt = fmt->format;
	} else {
		ret = s5kjn1_update_controls(s5kjn1, mode);
		if (!ret)
			s5kjn1->cur_mode = mode;
	}

	mutex_unlock(&s5kjn1->mutex);

	return ret;
}

/**
 * s5kjn1_init_state() - Initialize sub-device state
 * @sd: pointer to s5kjn1 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device configuration
 *
 * Return: 0 if successful, error code otherwise.
 */
static int s5kjn1_init_state(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *sd_state)
{
	struct s5kjn1 *s5kjn1 = to_s5kjn1(sd);
	struct v4l2_subdev_format fmt = { 0 };

	fmt.which = sd_state ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
	s5kjn1_fill_pad_format(s5kjn1, &supported_mode, &fmt);

	return s5kjn1_set_pad_format(sd, sd_state, &fmt);
}

/**
 * s5kjn1_start_streaming() - Start sensor stream
 * @s5kjn1: pointer to s5kjn1 device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int s5kjn1_start_streaming(struct s5kjn1 *s5kjn1)
{
	const struct s5kjn1_reg_list *reg_list;
	int ret;

	/* Write sensor mode registers */
	reg_list = &s5kjn1->cur_mode->reg_list;
	ret = s5kjn1_write_regs(s5kjn1, reg_list->regs,
				reg_list->num_of_regs);
	if (ret) {
		dev_err(s5kjn1->dev, "fail to write initial registers\n");
		return ret;
	}

	/* Setup handler will write actual exposure and gain */
	ret =  __v4l2_ctrl_handler_setup(s5kjn1->sd.ctrl_handler);
	if (ret) {
		dev_err(s5kjn1->dev, "fail to setup handler\n");
		return ret;
	}

	/* Delay is required before streaming*/
	usleep_range(7400, 8000);

	/* Start streaming */
	ret = s5kjn1_write_reg(s5kjn1, S5KJN1_REG_MODE_SELECT,
			       1, S5KJN1_MODE_STREAMING);
	if (ret) {
		dev_err(s5kjn1->dev, "fail to start streaming\n");
		return ret;
	}

	return 0;
}

/**
 * s5kjn1_stop_streaming() - Stop sensor stream
 * @s5kjn1: pointer to s5kjn1 device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int s5kjn1_stop_streaming(struct s5kjn1 *s5kjn1)
{
	return s5kjn1_write_reg(s5kjn1, S5KJN1_REG_MODE_SELECT,
				1, S5KJN1_MODE_STANDBY);
}

/**
 * s5kjn1_set_stream() - Enable sensor streaming
 * @sd: pointer to s5kjn1 subdevice
 * @enable: set to enable sensor streaming
 *
 * Return: 0 if successful, error code otherwise.
 */
static int s5kjn1_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct s5kjn1 *s5kjn1 = to_s5kjn1(sd);
	int ret;

	mutex_lock(&s5kjn1->mutex);

	if (enable) {
		ret = pm_runtime_resume_and_get(s5kjn1->dev);
		if (ret)
			goto error_unlock;

		ret = s5kjn1_start_streaming(s5kjn1);
		if (ret)
			goto error_power_off;
	} else {
		s5kjn1_stop_streaming(s5kjn1);
		pm_runtime_put(s5kjn1->dev);
	}

	mutex_unlock(&s5kjn1->mutex);

	return 0;

error_power_off:
	pm_runtime_put(s5kjn1->dev);
error_unlock:
	mutex_unlock(&s5kjn1->mutex);

	return ret;
}

/**
 * s5kjn1_detect() - Detect s5kjn1 sensor
 * @s5kjn1: pointer to s5kjn1 device
 *
 * Return: 0 if successful, -EIO if sensor id does not match
 */
static int s5kjn1_detect(struct s5kjn1 *s5kjn1)
{
	int ret;
	u32 val;

	ret = s5kjn1_read_reg(s5kjn1, S5KJN1_REG_ID, 2, &val);
	if (ret)
		return ret;

	if (val != S5KJN1_ID) {
		dev_err(s5kjn1->dev, "chip id mismatch: %x!=%x\n",
			S5KJN1_ID, val);
		return -ENXIO;
	}

	return 0;
}

/**
 * s5kjn1_parse_hw_config() - Parse HW configuration and check if supported
 * @s5kjn1: pointer to s5kjn1 device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int s5kjn1_parse_hw_config(struct s5kjn1 *s5kjn1)
{
	struct fwnode_handle *fwnode = dev_fwnode(s5kjn1->dev);
	struct v4l2_fwnode_endpoint bus_cfg = {
		.bus_type = V4L2_MBUS_CSI2_DPHY
	};
	struct fwnode_handle *ep;
	unsigned long rate;
	unsigned int i;
	int ret;

	if (!fwnode)
		return -ENXIO;

	/* Request optional reset pin */
	s5kjn1->reset_gpio = devm_gpiod_get_optional(s5kjn1->dev, "reset",
						     GPIOD_OUT_LOW);
	if (IS_ERR(s5kjn1->reset_gpio)) {
		dev_err(s5kjn1->dev, "failed to get reset gpio %ld\n",
			PTR_ERR(s5kjn1->reset_gpio));
		return PTR_ERR(s5kjn1->reset_gpio);
	}

	/* Get sensor input clock */
	s5kjn1->inclk = devm_clk_get(s5kjn1->dev, NULL);
	if (IS_ERR(s5kjn1->inclk)) {
		dev_err(s5kjn1->dev, "could not get inclk\n");
		return PTR_ERR(s5kjn1->inclk);
	}

	rate = clk_get_rate(s5kjn1->inclk);
	if (rate != S5KJN1_INCLK_RATE) {
		dev_err(s5kjn1->dev, "inclk frequency mismatch\n");
		return -EINVAL;
	}

	/* Get optional DT defined regulators */
	for (i = 0; i < ARRAY_SIZE(s5kjn1_supply_names); i++)
		s5kjn1->supplies[i].supply = s5kjn1_supply_names[i];

	ret = devm_regulator_bulk_get(s5kjn1->dev,
				      ARRAY_SIZE(s5kjn1_supply_names),
				      s5kjn1->supplies);
	if (ret)
		return ret;

	ep = fwnode_graph_get_next_endpoint(fwnode, NULL);
	if (!ep)
		return -ENXIO;

	ret = v4l2_fwnode_endpoint_alloc_parse(ep, &bus_cfg);
	fwnode_handle_put(ep);
	if (ret)
		return ret;

	if (bus_cfg.bus.mipi_csi2.num_data_lanes != S5KJN1_NUM_DATA_LANES) {
		dev_err(s5kjn1->dev,
			"number of CSI2 data lanes %d is not supported\n",
			bus_cfg.bus.mipi_csi2.num_data_lanes);
		ret = -EINVAL;
		goto done_endpoint_free;
	}

	if (!bus_cfg.nr_of_link_frequencies) {
		dev_err(s5kjn1->dev, "no link frequencies defined\n");
		ret = -EINVAL;
		goto done_endpoint_free;
	}

	for (i = 0; i < bus_cfg.nr_of_link_frequencies; i++)
		if (bus_cfg.link_frequencies[i] == S5KJN1_LINK_FREQ)
			goto done_endpoint_free;

	ret = -EINVAL;

done_endpoint_free:
	v4l2_fwnode_endpoint_free(&bus_cfg);

	return ret;
}

/* V4l2 subdevice ops */
static const struct v4l2_subdev_video_ops s5kjn1_video_ops = {
	.s_stream = s5kjn1_set_stream,
};

static const struct v4l2_subdev_pad_ops s5kjn1_pad_ops = {
	.enum_mbus_code = s5kjn1_enum_mbus_code,
	.enum_frame_size = s5kjn1_enum_frame_size,
	.get_fmt = s5kjn1_get_pad_format,
	.set_fmt = s5kjn1_set_pad_format,
};

static const struct v4l2_subdev_ops s5kjn1_subdev_ops = {
	.video = &s5kjn1_video_ops,
	.pad = &s5kjn1_pad_ops,
};

static const struct v4l2_subdev_internal_ops s5kjn1_internal_ops = {
	.init_state = s5kjn1_init_state,
};

/**
 * s5kjn1_power_on() - Sensor power on sequence
 * @dev: pointer to i2c device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int s5kjn1_power_on(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct s5kjn1 *s5kjn1 = to_s5kjn1(sd);
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(s5kjn1_supply_names),
				    s5kjn1->supplies);
	if (ret < 0) {
		dev_err(dev, "failed to enable regulators\n");
		return ret;
	}

	gpiod_set_value_cansleep(s5kjn1->reset_gpio, 0);

	ret = clk_prepare_enable(s5kjn1->inclk);
	if (ret) {
		dev_err(s5kjn1->dev, "fail to enable inclk\n");
		goto error_reset;
	}

	/* At least 45000 MCLK cycles */
	usleep_range(10000, 10200);

	return 0;

error_reset:
	gpiod_set_value_cansleep(s5kjn1->reset_gpio, 1);
	regulator_bulk_disable(ARRAY_SIZE(s5kjn1_supply_names),
			       s5kjn1->supplies);

	return ret;
}

/**
 * s5kjn1_power_off() - Sensor power off sequence
 * @dev: pointer to i2c device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int s5kjn1_power_off(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct s5kjn1 *s5kjn1 = to_s5kjn1(sd);

	clk_disable_unprepare(s5kjn1->inclk);

	gpiod_set_value_cansleep(s5kjn1->reset_gpio, 1);

	regulator_bulk_disable(ARRAY_SIZE(s5kjn1_supply_names),
			       s5kjn1->supplies);

	return 0;
}

/**
 * s5kjn1_init_controls() - Initialize sensor subdevice controls
 * @s5kjn1: pointer to s5kjn1 device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int s5kjn1_init_controls(struct s5kjn1 *s5kjn1)
{
	struct v4l2_fwnode_device_properties props;
	struct v4l2_ctrl_handler *ctrl_hdlr = &s5kjn1->ctrl_handler;
	const struct s5kjn1_mode *mode = s5kjn1->cur_mode;
	u32 lpfr;
	int ret;

	/* set properties from fwnode (e.g. rotation, orientation) */
	ret = v4l2_fwnode_device_parse(s5kjn1->dev, &props);
	if (ret)
		return ret;

	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 8);
	if (ret)
		return ret;

	/* Serialize controls with sensor device */
	ctrl_hdlr->lock = &s5kjn1->mutex;

	/* Initialize exposure and gain */
	lpfr = mode->vblank + mode->height;
	s5kjn1->exp_ctrl = v4l2_ctrl_new_std(ctrl_hdlr,
					     &s5kjn1_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     S5KJN1_EXPOSURE_MIN,
					     lpfr - S5KJN1_EXPOSURE_OFFSET,
					     S5KJN1_EXPOSURE_STEP,
					     S5KJN1_EXPOSURE_DEFAULT);

	s5kjn1->again_ctrl = v4l2_ctrl_new_std(ctrl_hdlr,
					       &s5kjn1_ctrl_ops,
					       V4L2_CID_ANALOGUE_GAIN,
					       S5KJN1_AGAIN_MIN,
					       S5KJN1_AGAIN_MAX,
					       S5KJN1_AGAIN_STEP,
					       S5KJN1_AGAIN_DEFAULT);

	v4l2_ctrl_cluster(2, &s5kjn1->exp_ctrl);

	s5kjn1->vblank_ctrl = v4l2_ctrl_new_std(ctrl_hdlr,
						&s5kjn1_ctrl_ops,
						V4L2_CID_VBLANK,
						mode->vblank_min,
						mode->vblank_max,
						1, mode->vblank);

	/* Read only controls */
	s5kjn1->pclk_ctrl = v4l2_ctrl_new_std(ctrl_hdlr,
					      &s5kjn1_ctrl_ops,
					      V4L2_CID_PIXEL_RATE,
					      mode->pclk, mode->pclk,
					      1, mode->pclk);

	s5kjn1->link_freq_ctrl = v4l2_ctrl_new_int_menu(ctrl_hdlr,
							&s5kjn1_ctrl_ops,
							V4L2_CID_LINK_FREQ,
							ARRAY_SIZE(link_freq) -
							1,
							mode->link_freq_idx,
							link_freq);
	if (s5kjn1->link_freq_ctrl)
		s5kjn1->link_freq_ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	s5kjn1->hblank_ctrl = v4l2_ctrl_new_std(ctrl_hdlr,
						&s5kjn1_ctrl_ops,
						V4L2_CID_HBLANK,
						S5KJN1_REG_MIN,
						S5KJN1_REG_MAX,
						1, mode->hblank);
	if (s5kjn1->hblank_ctrl)
		s5kjn1->hblank_ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	v4l2_ctrl_new_fwnode_properties(ctrl_hdlr, &s5kjn1_ctrl_ops, &props);

	if (ctrl_hdlr->error) {
		dev_err(s5kjn1->dev, "control init failed: %d\n",
			ctrl_hdlr->error);
		v4l2_ctrl_handler_free(ctrl_hdlr);
		return ctrl_hdlr->error;
	}

	s5kjn1->sd.ctrl_handler = ctrl_hdlr;

	return 0;
}

/**
 * s5kjn1_probe() - I2C client device binding
 * @client: pointer to i2c client device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int s5kjn1_probe(struct i2c_client *client)
{
	struct s5kjn1 *s5kjn1;
	int ret;

	s5kjn1 = devm_kzalloc(&client->dev, sizeof(*s5kjn1), GFP_KERNEL);
	if (!s5kjn1)
		return -ENOMEM;

	s5kjn1->dev = &client->dev;

	/* Initialize subdev */
	v4l2_i2c_subdev_init(&s5kjn1->sd, client, &s5kjn1_subdev_ops);
	s5kjn1->sd.internal_ops = &s5kjn1_internal_ops;

	ret = s5kjn1_parse_hw_config(s5kjn1);
	if (ret) {
		dev_err(s5kjn1->dev, "HW configuration is not supported\n");
		return ret;
	}

	mutex_init(&s5kjn1->mutex);

	ret = s5kjn1_power_on(s5kjn1->dev);
	if (ret) {
		dev_err(s5kjn1->dev, "failed to power-on the sensor\n");
		goto error_mutex_destroy;
	}

	/* Check module identity */
	ret = s5kjn1_detect(s5kjn1);
	if (ret) {
		dev_err(s5kjn1->dev, "failed to find sensor: %d\n", ret);
		goto error_power_off;
	}

	/* Set default mode to max resolution */
	s5kjn1->cur_mode = &supported_mode;
	s5kjn1->vblank = s5kjn1->cur_mode->vblank;

	ret = s5kjn1_init_controls(s5kjn1);
	if (ret) {
		dev_err(s5kjn1->dev, "failed to init controls: %d\n", ret);
		goto error_power_off;
	}

	/* Initialize subdev */
	s5kjn1->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	s5kjn1->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	/* Initialize source pad */
	s5kjn1->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&s5kjn1->sd.entity, 1, &s5kjn1->pad);
	if (ret) {
		dev_err(s5kjn1->dev, "failed to init entity pads: %d\n", ret);
		goto error_handler_free;
	}

	ret = v4l2_async_register_subdev_sensor(&s5kjn1->sd);
	if (ret < 0) {
		dev_err(s5kjn1->dev,
			"failed to register async subdev: %d\n", ret);
		goto error_media_entity;
	}

	pm_runtime_set_active(s5kjn1->dev);
	pm_runtime_enable(s5kjn1->dev);
	pm_runtime_idle(s5kjn1->dev);

	return 0;

error_media_entity:
	media_entity_cleanup(&s5kjn1->sd.entity);
error_handler_free:
	v4l2_ctrl_handler_free(s5kjn1->sd.ctrl_handler);
error_power_off:
	s5kjn1_power_off(s5kjn1->dev);
error_mutex_destroy:
	mutex_destroy(&s5kjn1->mutex);

	return ret;
}

static void s5kjn1_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5kjn1 *s5kjn1 = to_s5kjn1(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(sd->ctrl_handler);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		s5kjn1_power_off(&client->dev);
	pm_runtime_set_suspended(&client->dev);

	mutex_destroy(&s5kjn1->mutex);
}

static const struct dev_pm_ops s5kjn1_pm_ops = {
	SET_RUNTIME_PM_OPS(s5kjn1_power_off, s5kjn1_power_on, NULL)
};

static const struct of_device_id s5kjn1_of_match[] = {
	{ .compatible = "samsung,s5kjn1" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, s5kjn1_of_match);

static struct i2c_driver s5kjn1_driver = {
	.probe = s5kjn1_probe,
	.remove = s5kjn1_remove,
	.driver = {
		.name = "s5kjn1",
		.pm = &s5kjn1_pm_ops,
		.of_match_table = s5kjn1_of_match,
	},
};

module_i2c_driver(s5kjn1_driver);

MODULE_DESCRIPTION("Samsung S5KJN1 sensor driver");
MODULE_LICENSE("GPL");
