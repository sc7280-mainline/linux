// SPDX-License-Identifier: GPL-2.0-only
/*
 * Sony IMX800 Camera Sensor Driver
 *
 * Copyright (C) 2021 Intel Corporation
 */
#include <asm/unaligned.h>

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
#define IMX800_REG_MODE_SELECT	0x0100
#define IMX800_MODE_STANDBY	0x00
#define IMX800_MODE_STREAMING	0x01

/* Lines per frame */
#define IMX800_REG_LPFR		0x0340

/* Chip ID */
#define IMX800_REG_ID		0x0016
#define IMX800_ID		0x800

//TODO check values
/* Exposure control */
#define IMX800_REG_EXPOSURE_CIT	0x0202
#define IMX800_EXPOSURE_MIN	8
#define IMX800_EXPOSURE_OFFSET	22
#define IMX800_EXPOSURE_STEP	1
#define IMX800_EXPOSURE_DEFAULT	0x0648

//TODO check min and max
/* Analog gain control */
#define IMX800_REG_AGAIN	0x0204
#define IMX800_AGAIN_MIN	0
#define IMX800_AGAIN_MAX	978
#define IMX800_AGAIN_STEP	1
#define IMX800_AGAIN_DEFAULT	0

//TODO
/*
test pattern 0x601 or 0x600
flip 0x101
 */

/* Group hold register */
#define IMX800_REG_HOLD		0x0104

/* Input clock rate */
#define IMX800_INCLK_RATE	24000000

/* CSI2 HW configuration */
#define IMX800_LINK_FREQ	600000000
#define IMX800_NUM_DATA_LANES	4

#define IMX800_REG_MIN		0x00
#define IMX800_REG_MAX		0xffff

/**
 * struct imx800_reg - imx800 sensor register
 * @address: Register address
 * @val: Register value
 */
struct imx800_reg {
	u16 address;
	u8 val;
};

/**
 * struct imx800_reg_list - imx800 sensor register list
 * @num_of_regs: Number of registers in the list
 * @regs: Pointer to register list
 */
struct imx800_reg_list {
	u32 num_of_regs;
	const struct imx800_reg *regs;
};

/**
 * struct imx800_mode - imx800 sensor mode structure
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
struct imx800_mode {
	u32 width;
	u32 height;
	u32 code;
	u32 hblank;
	u32 vblank;
	u32 vblank_min;
	u32 vblank_max;
	u64 pclk;
	u32 link_freq_idx;
	struct imx800_reg_list reg_list;
};

static const char * const imx800_supply_names[] = {
	"dovdd",	/* Digital I/O power */
	"avdd",		/* Analog power */
	"dvdd",		/* Digital core power */
};

/**
 * struct imx800 - imx800 sensor device structure
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
 * @streaming: Flag indicating streaming state
 */
struct imx800 {
	struct device *dev;
	struct i2c_client *client;
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct gpio_desc *reset_gpio;
	struct clk *inclk;
	struct regulator_bulk_data supplies[ARRAY_SIZE(imx800_supply_names)];
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
	const struct imx800_mode *cur_mode;
	struct mutex mutex;
	bool streaming;
};

static const s64 link_freq[] = {
	IMX800_LINK_FREQ,
};

/* Sensor mode registers */
static const struct imx800_reg mode_4096x3072_regs[] = {
	//Power ON
	//Input EXTCLK
	//XCLR OFF
	//External Clock Setting
	{0x0136, 0x18},
	{0x0137, 0x00},
	{0x3304, 0x00},
	{0x33f0, 0x02},
	{0x33f1, 0x03},
	{0x0111, 0x03},
	{0x3101, 0x0f},
	{0x316e, 0x00},
	{0x3379, 0x00},
	{0x39d1, 0x00},
	{0x3a34, 0x00},
	{0x3a35, 0xe6},
	{0x3a36, 0x00},
	{0x3a37, 0xe6},
	{0x4a83, 0x66},
	{0x4a97, 0x52},
	{0x4aa5, 0x6a},
	{0x4aab, 0x66},
	{0x4abf, 0x52},
	{0x4acd, 0x6a},
	{0x4ad5, 0x6a},
	{0x4b57, 0x66},
	{0x4b6b, 0x52},
	{0x4b7b, 0x6a},
	{0x56d8, 0x2f},
	{0x56da, 0x2f},
	{0x56dc, 0x2f},
	{0x56e0, 0x2f},
	{0x56e2, 0x2f},
	{0x56e3, 0x2f},
	{0x56e5, 0x2f},
	{0x56e7, 0x2f},
	{0x56e9, 0x2f},
	{0x56eb, 0x2f},
	{0x56ed, 0x2f},
	{0x56ef, 0x2f},
	{0x5829, 0x06},
	{0x5844, 0x06},
	{0x5944, 0x00},
	{0x5948, 0x00},
	{0x594c, 0x00},
	{0x594e, 0x00},
	{0x594f, 0x00},
	{0x5953, 0x00},
	{0x5955, 0x00},
	{0x5957, 0x00},
	{0x5959, 0x00},
	{0x595b, 0x00},
	{0x5964, 0x00},
	{0x5966, 0x00},
	{0x5967, 0x00},
	{0x5969, 0x00},
	{0x596a, 0x00},
	{0x596b, 0x00},
	{0x596c, 0x00},
	{0x596d, 0x00},
	{0x5975, 0x00},
	{0x5979, 0x00},
	{0x5980, 0x00},
	{0x5982, 0x00},
	{0x5cc4, 0x20},
	{0x5cc8, 0x09},
	{0x5ccc, 0x09},
	{0x5cce, 0x09},
	{0x5ccf, 0x15},
	{0x5cd3, 0x04},
	{0x5cd5, 0x04},
	{0x5cd7, 0x09},
	{0x5cd9, 0x04},
	{0x5cdb, 0x04},
	{0x5ce4, 0x09},
	{0x5ce6, 0x09},
	{0x5ce7, 0x09},
	{0x5ce9, 0x04},
	{0x5cea, 0x04},
	{0x5ceb, 0x09},
	{0x5cec, 0x04},
	{0x5ced, 0x04},
	{0x5cf1, 0x28},
	{0x5cf5, 0x09},
	{0x5cf9, 0x09},
	{0x5cfb, 0x1f},
	{0x5cfc, 0x1a},
	{0x5d00, 0x04},
	{0x5d02, 0x04},
	{0x5d04, 0x1f},
	{0x5d06, 0x15},
	{0x5d08, 0x15},
	{0x5d10, 0x1f},
	{0x5d12, 0x1f},
	{0x5d13, 0x15},
	{0x5d14, 0x15},
	{0x5d15, 0x26},
	{0x5d19, 0x1f},
	{0x5d1d, 0x1f},
	{0x5d1f, 0x2d},
	{0x5d20, 0x1a},
	{0x5d24, 0x15},
	{0x5d26, 0x15},
	{0x5d28, 0x2d},
	{0x5d2a, 0x1a},
	{0x5d2c, 0x1a},
	{0x5d34, 0x2d},
	{0x5d38, 0x2d},
	{0x5d3c, 0x2d},
	{0x5d3e, 0x2d},
	{0x5d3f, 0x1e},
	{0x5d43, 0x1a},
	{0x5d45, 0x1a},
	{0x5d47, 0x2d},
	{0x5d49, 0x1a},
	{0x5d4b, 0x1a},
	{0x5d54, 0x2d},
	{0x5d56, 0x2d},
	{0x5d57, 0x2d},
	{0x5d59, 0x1a},
	{0x5d5a, 0x1a},
	{0x5d5b, 0x2d},
	{0x5d5c, 0x1e},
	{0x5d5d, 0x1e},
	{0x5d61, 0x2d},
	{0x5d63, 0x2d},
	{0x5d64, 0x1e},
	{0x5d65, 0x1e},
	{0x5d66, 0x28},
	{0x5d6a, 0x0f},
	{0x5d6e, 0x0f},
	{0x5d70, 0x0f},
	{0x5d71, 0x6c},
	{0x5d75, 0x2d},
	{0x5d77, 0x2d},
	{0x5d79, 0x0f},
	{0x5d7b, 0x2d},
	{0x5d7d, 0x2d},
	{0x5d86, 0x0f},
	{0x5d88, 0x0f},
	{0x5d89, 0x0f},
	{0x5d8b, 0x2d},
	{0x5d8c, 0x2d},
	{0x5d8d, 0x0f},
	{0x5d8e, 0x2d},
	{0x5d8f, 0x2d},
	{0x5d93, 0x79},
	{0x5d97, 0x0f},
	{0x5d9b, 0x0f},
	{0x5d9d, 0x2d},
	{0x5d9e, 0x7b},
	{0x5da2, 0x2d},
	{0x5da4, 0x2d},
	{0x5da6, 0x2d},
	{0x5da8, 0x6c},
	{0x5daa, 0x6c},
	{0x5db2, 0x2d},
	{0x5db4, 0x2d},
	{0x5db5, 0x6c},
	{0x5db6, 0x6c},
	{0x5db7, 0x5a},
	{0x5dbb, 0x2d},
	{0x5dbf, 0x2d},
	{0x5dc1, 0x32},
	{0x5dc2, 0x7d},
	{0x5dc6, 0x6c},
	{0x5dc8, 0x6c},
	{0x5dca, 0x32},
	{0x5dcc, 0x7b},
	{0x5dce, 0x7b},
	{0x5dd6, 0x2d},
	{0x5dda, 0x32},
	{0x5dde, 0x32},
	{0x5de0, 0x1e},
	{0x5de1, 0x7a},
	{0x5de5, 0x7b},
	{0x5de7, 0x7b},
	{0x5de9, 0x1e},
	{0x5deb, 0x7d},
	{0x5ded, 0x7d},
	{0x5df6, 0x1e},
	{0x5df8, 0x1e},
	{0x5df9, 0x28},
	{0x5dfb, 0x7d},
	{0x5dfc, 0x7d},
	{0x5dfd, 0x28},
	{0x5dfe, 0x7a},
	{0x5dff, 0x7a},
	{0x5e03, 0x28},
	{0x5e05, 0x28},
	{0x5e06, 0x7a},
	{0x5e07, 0x7a},
	{0x6132, 0x2d},
	{0x6136, 0x0f},
	{0x613a, 0x0f},
	{0x613c, 0x23},
	{0x613d, 0x2d},
	{0x6141, 0x0f},
	{0x6143, 0x0f},
	{0x6145, 0x23},
	{0x6147, 0x23},
	{0x6149, 0x23},
	{0x6151, 0x2d},
	{0x6155, 0x0f},
	{0x6159, 0x0f},
	{0x615b, 0x23},
	{0x615c, 0x2d},
	{0x6160, 0x0f},
	{0x6162, 0x0f},
	{0x6164, 0x23},
	{0x6166, 0x23},
	{0x6168, 0x23},
	{0x6174, 0x0f},
	{0x6178, 0x0f},
	{0x617a, 0x2d},
	{0x617f, 0x0f},
	{0x6181, 0x0f},
	{0x6183, 0x2d},
	{0x6185, 0x2d},
	{0x6187, 0x2d},
	{0x6193, 0x23},
	{0x6197, 0x23},
	{0x6199, 0x2d},
	{0x619e, 0x23},
	{0x61a0, 0x23},
	{0x61a2, 0x2d},
	{0x61a4, 0x2d},
	{0x61a6, 0x2d},
	{0x61b2, 0x23},
	{0x61b6, 0x23},
	{0x61bd, 0x23},
	{0x61bf, 0x23},
	{0x6201, 0x28},
	{0x6203, 0x28},
	{0x6207, 0x28},
	{0x6209, 0x28},
	{0x620c, 0x28},
	{0x620e, 0x28},
	{0x6210, 0x28},
	{0x6212, 0x28},
	{0x6214, 0x28},
	{0x6216, 0x28},
	{0x6220, 0x28},
	{0x6222, 0x28},
	{0x6224, 0x28},
	{0x6226, 0x28},
	{0x6229, 0x28},
	{0x622b, 0x28},
	{0x622f, 0x28},
	{0x6231, 0x28},
	{0x6234, 0x28},
	{0x6236, 0x28},
	{0x6238, 0x28},
	{0x623a, 0x28},
	{0x623c, 0x28},
	{0x623e, 0x28},
	{0x636b, 0x02},
	{0x636f, 0x02},
	{0x6376, 0x02},
	{0x6378, 0x02},
	{0x638a, 0x11},
	{0x638e, 0x11},
	{0x63a6, 0x11},
	{0x63a8, 0x11},
	{0x63b7, 0x11},
	{0x63bb, 0x11},
	{0x63d2, 0x11},
	{0x63d4, 0x11},
	{0x63d5, 0x13},
	{0x63d6, 0x13},
	{0x63db, 0x11},
	{0x63df, 0x11},
	{0x63e6, 0x13},
	{0x63e8, 0x13},
	{0x63fa, 0x11},
	{0x63fe, 0x11},
	{0x6429, 0x20},
	{0x642d, 0x20},
	{0x6431, 0x19},
	{0x6439, 0x19},
	{0x643d, 0x20},
	{0x643f, 0x20},
	{0x6443, 0x20},
	{0x6447, 0x19},
	{0x644b, 0x19},
	{0x644f, 0x20},
	{0x6453, 0x20},
	{0x6457, 0x20},
	{0x6467, 0x20},
	{0x6469, 0x19},
	{0x646d, 0x19},
	{0x646f, 0x20},
	{0x6471, 0x20},
	{0x6473, 0x19},
	{0x6475, 0x19},
	{0x6477, 0x20},
	{0x6479, 0x20},
	{0x647b, 0x20},
	{0x6487, 0x20},
	{0x648b, 0x20},
	{0x6493, 0x20},
	{0x6497, 0x20},
	{0x649d, 0x20},
	{0x64a1, 0x20},
	{0x64a5, 0x20},
	{0x64a9, 0x20},
	{0x64ad, 0x20},
	{0x64b1, 0x20},
	{0x64c1, 0x21},
	{0x64c5, 0x21},
	{0x64c7, 0x21},
	{0x64c9, 0x21},
	{0x64d3, 0x21},
	{0x64db, 0x21},
	{0x64e9, 0x21},
	{0x64ed, 0x21},
	{0x656d, 0x61},
	{0x6571, 0x61},
	{0x6575, 0x5a},
	{0x657d, 0x5a},
	{0x6581, 0x61},
	{0x6583, 0x61},
	{0x6587, 0x61},
	{0x658b, 0x5a},
	{0x658f, 0x5a},
	{0x6593, 0x61},
	{0x6597, 0x61},
	{0x659b, 0x61},
	{0x65ab, 0x61},
	{0x65ad, 0x5a},
	{0x65b1, 0x5a},
	{0x65b3, 0x61},
	{0x65b5, 0x61},
	{0x65b7, 0x5a},
	{0x65b9, 0x5a},
	{0x65bb, 0x61},
	{0x65bd, 0x61},
	{0x65bf, 0x61},
	{0x65cb, 0x61},
	{0x65cf, 0x61},
	{0x65d7, 0x61},
	{0x65db, 0x61},
	{0x65e1, 0x61},
	{0x65e5, 0x61},
	{0x65e9, 0x61},
	{0x65ed, 0x61},
	{0x65f1, 0x61},
	{0x65f5, 0x61},
	{0x6605, 0x62},
	{0x6609, 0x62},
	{0x660b, 0x62},
	{0x660d, 0x62},
	{0x6617, 0x62},
	{0x661f, 0x62},
	{0x662d, 0x62},
	{0x6631, 0x62},
	{0x66b1, 0x61},
	{0x66b5, 0x5a},
	{0x66b9, 0x61},
	{0x66c1, 0x5a},
	{0x66c5, 0x61},
	{0x66c7, 0x5a},
	{0x66c9, 0x61},
	{0x66cb, 0x5a},
	{0x66cd, 0x61},
	{0x66d3, 0x61},
	{0x66d7, 0x61},
	{0x66df, 0x61},
	{0x66e3, 0x61},
	{0x66e5, 0x62},
	{0x66e7, 0x62},
	{0x66ed, 0x62},
	{0x66f9, 0x62},
	{0x6720, 0x1e},
	{0x6724, 0x1e},
	{0x6728, 0x1e},
	{0x672a, 0x1e},
	{0x672b, 0x1e},
	{0x672f, 0x1e},
	{0x6731, 0x1e},
	{0x6733, 0x1e},
	{0x6735, 0x1e},
	{0x6737, 0x1e},
	{0x6740, 0x1e},
	{0x6742, 0x1e},
	{0x6743, 0x1e},
	{0x6745, 0x1e},
	{0x6746, 0x1e},
	{0x6747, 0x1e},
	{0x6748, 0x1e},
	{0x6749, 0x1e},
	{0x674d, 0x1e},
	{0x6751, 0x1e},
	{0x6755, 0x1e},
	{0x6757, 0x1e},
	{0x6758, 0x1e},
	{0x675c, 0x1e},
	{0x675e, 0x1e},
	{0x6760, 0x1e},
	{0x6762, 0x1e},
	{0x6764, 0x1e},
	{0x676c, 0x1e},
	{0x676e, 0x1e},
	{0x676f, 0x1e},
	{0x6770, 0x1e},
	{0x6771, 0x1e},
	{0x6775, 0x1e},
	{0x6779, 0x1e},
	{0x677b, 0x1e},
	{0x677c, 0x1e},
	{0x6780, 0x1e},
	{0x6782, 0x1e},
	{0x6784, 0x1e},
	{0x6786, 0x1e},
	{0x6788, 0x1e},
	{0x6790, 0x1e},
	{0x6794, 0x1e},
	{0x6798, 0x1e},
	{0x679a, 0x1e},
	{0x679b, 0x1e},
	{0x679f, 0x1e},
	{0x67a1, 0x1e},
	{0x67a3, 0x1e},
	{0x67a5, 0x1e},
	{0x67a7, 0x1e},
	{0x67b0, 0x1e},
	{0x67b2, 0x1e},
	{0x67b3, 0x1e},
	{0x67b5, 0x1e},
	{0x67b6, 0x1e},
	{0x67b7, 0x1e},
	{0x67b8, 0x1e},
	{0x67bc, 0x1e},
	{0x67be, 0x1e},
	{0x67bf, 0x1e},
	{0x67c0, 0x1e},
	{0x7514, 0x08},
	{0x7732, 0x03},
	{0x7bb3, 0x40},
	{0x7bb7, 0xbf},
	{0x7bba, 0x01},
	{0x7bbb, 0x03},
	{0x7bbc, 0x81},
	{0x7bbd, 0x08},
	{0x7bbe, 0x81},
	{0x7bbf, 0x18},
	{0x7bc0, 0x48},
	{0x7bc1, 0x48},
	{0x7bc2, 0x48},
	{0x7bc3, 0x58},
	{0x7bc4, 0x46},
	{0x7bc5, 0x80},
	{0x7bc6, 0x46},
	{0x7bc7, 0x90},
	{0x7bc8, 0x8f},
	{0x7bc9, 0xc0},
	{0x7bca, 0x8f},
	{0x7bcb, 0xd0},
	{0x7d67, 0x40},
	{0x7d6b, 0xbf},
	{0x7d6e, 0x01},
	{0x7d6f, 0x03},
	{0x7d90, 0x81},
	{0x7d91, 0x08},
	{0x7d92, 0x81},
	{0x7d93, 0x18},
	{0x7d94, 0x48},
	{0x7d95, 0x48},
	{0x7d96, 0x48},
	{0x7d97, 0x58},
	{0x7d98, 0x46},
	{0x7d99, 0x80},
	{0x7d9a, 0x46},
	{0x7d9b, 0x90},
	{0x7d9c, 0x8f},
	{0x7d9d, 0xc0},
	{0x7d9e, 0x8f},
	{0x7d9f, 0xd0},
	{0x86a9, 0x4e},
	{0x9002, 0x08},
	{0x9003, 0x08},
	{0x9004, 0x10},
	{0x90b7, 0xb0},
	{0x90b9, 0xb0},
	{0x90d7, 0xb0},
	{0x90e4, 0x08},
	{0x90e5, 0x08},
	{0x90e6, 0x10},
	{0x9230, 0xbd},
	{0x9231, 0x07},
	{0x9232, 0xbd},
	{0x9233, 0x01},
	{0x9234, 0xbd},
	{0x9235, 0x02},
	{0x9236, 0x86},
	{0x9237, 0xda},
	{0x9238, 0xb5},
	{0x9239, 0x22},
	{0x923a, 0xb6},
	{0x923b, 0x6d},
	{0x923c, 0x31},
	{0x923d, 0xbc},
	{0x923e, 0xb5},
	{0x923f, 0x23},
	{0xb507, 0x40},
	{0xb50b, 0xbf},
	{0xb50e, 0x03},
	{0xbcaf, 0x01},
	{0xbd4e, 0x0f},
	{0xbd4f, 0x20},
	{0xbd56, 0x0f},
	{0xbd57, 0x20},
	{0xbd5a, 0x0f},
	{0xbd5b, 0x20},
	{0xbd62, 0x11},
	{0xbd63, 0xb0},
	{0xbd66, 0x2d},
	{0xbd6a, 0x11},
	{0xbd6b, 0xb0},
	{0xbd6e, 0x2d},
	{0xbd73, 0x00},
	{0xbd7b, 0x00},
	{0xbd7f, 0x00},
	{0xbd82, 0x1b},
	{0xbd86, 0x36},
	{0xbd8a, 0x1b},
	{0xbd8e, 0x36},
	{0xbd92, 0x0f},
	{0xbd93, 0x20},
	{0xbd97, 0x00},
	{0xbd9a, 0x1b},
	{0xbd9e, 0x36},
	{0xbdb6, 0x11},
	{0xbdb7, 0xb0},
	{0xbdba, 0x2d},
	{0x3209, 0x01},
	{0x3533, 0x10},
	{0xa248, 0x10},
	{0xa249, 0x10},
	{0xa24a, 0x10},
	{0xa71b, 0x0d},
	{0xa71d, 0x10},
	{0xa721, 0x0d},
	{0xa723, 0x10},
	{0xa727, 0x0d},
	{0xa729, 0x10},
	{0xa73f, 0x9d},
	{0xa741, 0xc4},
	{0xa745, 0x9d},
	{0xa747, 0xc4},
	{0xa74b, 0x9d},
	{0xa74d, 0xc4},
	{0xa751, 0x0a},
	{0xa753, 0x0b},
	{0xa757, 0x0a},
	{0xa759, 0x0b},
	{0xa75d, 0x0a},
	{0xa75f, 0x0b},
	{0xa761, 0xb4},
	{0xa763, 0x56},
	{0xa767, 0xb4},
	{0xa769, 0x56},
	{0xa76d, 0xb4},
	{0xa76f, 0x56},
	{0xc06d, 0x0a},
	{0xc06e, 0x0a},
	{0xc070, 0x0a},
	{0xc071, 0x0a},
	{0xc073, 0x0a},
	{0xc074, 0x0a},
	{0xc076, 0x0a},
	{0xc077, 0x0a},
	{0xc086, 0x02},
	{0xc089, 0x02},
	{0xc08c, 0x02},
	{0xc08f, 0x02},
	{0xc0b5, 0x0a},
	{0xc0b6, 0x0a},
	{0xc0b8, 0x0a},
	{0xc0b9, 0x0a},
	{0xc0bb, 0x0a},
	{0xc0bc, 0x0a},
	{0xc0be, 0x0a},
	{0xc0bf, 0x0a},
	{0xc0ce, 0x02},
	{0xc0d1, 0x02},
	{0xc0d4, 0x02},
	{0xc0d7, 0x02},
	{0xcb89, 0x2d},
	{0xcb8f, 0x2d},
	{0xcbab, 0x23},
	{0xcbad, 0x23},
	{0xcbb1, 0x23},
	{0xcbb3, 0x23},
	{0xcc71, 0x2d},
	{0xcc77, 0x2d},
	{0xcc93, 0x23},
	{0xcc95, 0x23},
	{0xcc99, 0x23},
	{0xcc9b, 0x23},
	{0xcd71, 0x51},
	{0xcd77, 0x51},
	{0xcd7d, 0x51},
	{0xcd83, 0x51},
	{0xf006, 0x03},
	{0xf007, 0x20},
	{0xf008, 0x03},
	{0xf009, 0x20},
	{0xf00a, 0x03},
	{0xf00b, 0x20},
	{0xf012, 0x03},
	{0xf013, 0x84},
	{0xf014, 0x03},
	{0xf015, 0x84},
	{0xf016, 0x03},
	{0xf017, 0x84},
	{0xf072, 0x03},
	{0xf073, 0x20},
	{0xf074, 0x03},
	{0xf075, 0x20},
	{0xf076, 0x03},
	{0xf077, 0x20},
	{0xf07f, 0x84},
	{0xf081, 0x84},
	{0xf083, 0x84},
	{0xf501, 0x01},
};

//TODO check blank ad format values
/* Supported sensor mode configurations */
static const struct imx800_mode supported_mode = {
	.width = 4096,
	.height = 3072,
	.hblank = 456,
	.vblank = 506,
	.vblank_min = 506,
	.vblank_max = 32420,
	.pclk = 480000000,
	.link_freq_idx = 0,
	.code = MEDIA_BUS_FMT_SRGGB10_1X10,
	.reg_list = {
		.num_of_regs = ARRAY_SIZE(mode_4096x3072_regs),
		.regs = mode_4096x3072_regs,
	},
};

/**
 * to_imx800() - imx800 V4L2 sub-device to imx800 device.
 * @subdev: pointer to imx800 V4L2 sub-device
 *
 * Return: pointer to imx800 device
 */
static inline struct imx800 *to_imx800(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct imx800, sd);
}

/**
 * imx800_read_reg() - Read registers.
 * @imx800: pointer to imx800 device
 * @reg: register address
 * @len: length of bytes to read. Max supported bytes is 4
 * @val: pointer to register value to be filled.
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx800_read_reg(struct imx800 *imx800, u16 reg, u32 len, u32 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx800->sd);
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
 * imx800_write_reg() - Write register
 * @imx800: pointer to imx800 device
 * @reg: register address
 * @len: length of bytes. Max supported bytes is 4
 * @val: register value
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx800_write_reg(struct imx800 *imx800, u16 reg, u32 len, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx800->sd);
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
 * imx800_write_regs() - Write a list of registers
 * @imx800: pointer to imx800 device
 * @regs: list of registers to be written
 * @len: length of registers array
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx800_write_regs(struct imx800 *imx800,
			     const struct imx800_reg *regs, u32 len)
{
	unsigned int i;
	int ret;

	for (i = 0; i < len; i++) {
		ret = imx800_write_reg(imx800, regs[i].address, 1, regs[i].val);
		if (ret)
			return ret;
	}

	return 0;
}

/**
 * imx800_update_controls() - Update control ranges based on streaming mode
 * @imx800: pointer to imx800 device
 * @mode: pointer to imx800_mode sensor mode
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx800_update_controls(struct imx800 *imx800,
				  const struct imx800_mode *mode)
{
	int ret;

	ret = __v4l2_ctrl_s_ctrl(imx800->link_freq_ctrl, mode->link_freq_idx);
	if (ret)
		return ret;

	ret = __v4l2_ctrl_s_ctrl(imx800->hblank_ctrl, mode->hblank);
	if (ret)
		return ret;

	return __v4l2_ctrl_modify_range(imx800->vblank_ctrl, mode->vblank_min,
					mode->vblank_max, 1, mode->vblank);
}

/**
 * imx800_update_exp_gain() - Set updated exposure and gain
 * @imx800: pointer to imx800 device
 * @exposure: updated exposure value
 * @gain: updated analog gain value
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx800_update_exp_gain(struct imx800 *imx800, u32 exposure, u32 gain)
{
	u32 lpfr, shutter;
	int ret;

	lpfr = imx800->vblank + imx800->cur_mode->height;
	shutter = lpfr - exposure;

	dev_dbg(imx800->dev, "Set exp %u, analog gain %u, shutter %u, lpfr %u",
		exposure, gain, shutter, lpfr);

	ret = imx800_write_reg(imx800, IMX800_REG_HOLD, 1, 1);
	if (ret)
		return ret;

	ret = imx800_write_reg(imx800, IMX800_REG_LPFR, 2, lpfr);
	if (ret)
		goto error_release_group_hold;

	ret = imx800_write_reg(imx800, IMX800_REG_EXPOSURE_CIT, 2, shutter);
	if (ret)
		goto error_release_group_hold;

	ret = imx800_write_reg(imx800, IMX800_REG_AGAIN, 2, gain);

error_release_group_hold:
	imx800_write_reg(imx800, IMX800_REG_HOLD, 1, 0);

	return ret;
}

/**
 * imx800_set_ctrl() - Set subdevice control
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
static int imx800_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx800 *imx800 =
		container_of(ctrl->handler, struct imx800, ctrl_handler);
	u32 analog_gain;
	u32 exposure;
	int ret;

	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		imx800->vblank = imx800->vblank_ctrl->val;

		dev_dbg(imx800->dev, "Received vblank %u, new lpfr %u",
			imx800->vblank,
			imx800->vblank + imx800->cur_mode->height);

		ret = __v4l2_ctrl_modify_range(imx800->exp_ctrl,
					       IMX800_EXPOSURE_MIN,
					       imx800->vblank +
					       imx800->cur_mode->height -
					       IMX800_EXPOSURE_OFFSET,
					       1, IMX800_EXPOSURE_DEFAULT);
		break;
	case V4L2_CID_EXPOSURE:
		/* Set controls only if sensor is in power on state */
		if (!pm_runtime_get_if_in_use(imx800->dev))
			return 0;

		exposure = ctrl->val;
		analog_gain = imx800->again_ctrl->val;

		dev_dbg(imx800->dev, "Received exp %u, analog gain %u",
			exposure, analog_gain);

		ret = imx800_update_exp_gain(imx800, exposure, analog_gain);

		pm_runtime_put(imx800->dev);

		break;
	default:
		dev_err(imx800->dev, "Invalid control %d", ctrl->id);
		ret = -EINVAL;
	}

	return ret;
}

/* V4l2 subdevice control ops*/
static const struct v4l2_ctrl_ops imx800_ctrl_ops = {
	.s_ctrl = imx800_set_ctrl,
};

/**
 * imx800_enum_mbus_code() - Enumerate V4L2 sub-device mbus codes
 * @sd: pointer to imx800 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device configuration
 * @code: V4L2 sub-device code enumeration need to be filled
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx800_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index > 0)
		return -EINVAL;

	code->code = supported_mode.code;

	return 0;
}

/**
 * imx800_enum_frame_size() - Enumerate V4L2 sub-device frame sizes
 * @sd: pointer to imx800 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device configuration
 * @fsize: V4L2 sub-device size enumeration need to be filled
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx800_enum_frame_size(struct v4l2_subdev *sd,
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
 * imx800_fill_pad_format() - Fill subdevice pad format
 *                            from selected sensor mode
 * @imx800: pointer to imx800 device
 * @mode: pointer to imx800_mode sensor mode
 * @fmt: V4L2 sub-device format need to be filled
 */
static void imx800_fill_pad_format(struct imx800 *imx800,
				   const struct imx800_mode *mode,
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
 * imx800_get_pad_format() - Get subdevice pad format
 * @sd: pointer to imx800 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device configuration
 * @fmt: V4L2 sub-device format need to be set
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx800_get_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_format *fmt)
{
	struct imx800 *imx800 = to_imx800(sd);

	mutex_lock(&imx800->mutex);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		struct v4l2_mbus_framefmt *framefmt;

		framefmt = v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
		fmt->format = *framefmt;
	} else {
		imx800_fill_pad_format(imx800, imx800->cur_mode, fmt);
	}

	mutex_unlock(&imx800->mutex);

	return 0;
}

/**
 * imx800_set_pad_format() - Set subdevice pad format
 * @sd: pointer to imx800 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device configuration
 * @fmt: V4L2 sub-device format need to be set
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx800_set_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_format *fmt)
{
	struct imx800 *imx800 = to_imx800(sd);
	const struct imx800_mode *mode;
	int ret = 0;

	mutex_lock(&imx800->mutex);

	mode = &supported_mode;
	imx800_fill_pad_format(imx800, mode, fmt);

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		struct v4l2_mbus_framefmt *framefmt;

		framefmt = v4l2_subdev_get_try_format(sd, sd_state, fmt->pad);
		*framefmt = fmt->format;
	} else {
		ret = imx800_update_controls(imx800, mode);
		if (!ret)
			imx800->cur_mode = mode;
	}

	mutex_unlock(&imx800->mutex);

	return ret;
}

/**
 * imx800_init_pad_cfg() - Initialize sub-device pad configuration
 * @sd: pointer to imx800 V4L2 sub-device structure
 * @sd_state: V4L2 sub-device configuration
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx800_init_pad_cfg(struct v4l2_subdev *sd,
			       struct v4l2_subdev_state *sd_state)
{
	struct imx800 *imx800 = to_imx800(sd);
	struct v4l2_subdev_format fmt = { 0 };

	fmt.which = sd_state ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
	imx800_fill_pad_format(imx800, &supported_mode, &fmt);

	return imx800_set_pad_format(sd, sd_state, &fmt);
}

/**
 * imx800_start_streaming() - Start sensor stream
 * @imx800: pointer to imx800 device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx800_start_streaming(struct imx800 *imx800)
{
	const struct imx800_reg_list *reg_list;
	int ret;

	/* Write sensor mode registers */
	reg_list = &imx800->cur_mode->reg_list;
	ret = imx800_write_regs(imx800, reg_list->regs,
				reg_list->num_of_regs);
	if (ret) {
		dev_err(imx800->dev, "fail to write initial registers");
		return ret;
	}

	/* Setup handler will write actual exposure and gain */
	ret =  __v4l2_ctrl_handler_setup(imx800->sd.ctrl_handler);
	if (ret) {
		dev_err(imx800->dev, "fail to setup handler");
		return ret;
	}

	/* Delay is required before streaming*/
	usleep_range(7400, 8000);

	/* Start streaming */
	ret = imx800_write_reg(imx800, IMX800_REG_MODE_SELECT,
			       1, IMX800_MODE_STREAMING);
	if (ret) {
		dev_err(imx800->dev, "fail to start streaming");
		return ret;
	}

	return 0;
}

/**
 * imx800_stop_streaming() - Stop sensor stream
 * @imx800: pointer to imx800 device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx800_stop_streaming(struct imx800 *imx800)
{
	return imx800_write_reg(imx800, IMX800_REG_MODE_SELECT,
				1, IMX800_MODE_STANDBY);
}

/**
 * imx800_set_stream() - Enable sensor streaming
 * @sd: pointer to imx800 subdevice
 * @enable: set to enable sensor streaming
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx800_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct imx800 *imx800 = to_imx800(sd);
	int ret;

	mutex_lock(&imx800->mutex);

	if (imx800->streaming == enable) {
		mutex_unlock(&imx800->mutex);
		return 0;
	}

	if (enable) {
		ret = pm_runtime_resume_and_get(imx800->dev);
		if (ret)
			goto error_unlock;

		ret = imx800_start_streaming(imx800);
		if (ret)
			goto error_power_off;
	} else {
		imx800_stop_streaming(imx800);
		pm_runtime_put(imx800->dev);
	}

	imx800->streaming = enable;

	mutex_unlock(&imx800->mutex);

	return 0;

error_power_off:
	pm_runtime_put(imx800->dev);
error_unlock:
	mutex_unlock(&imx800->mutex);

	return ret;
}

/**
 * imx800_detect() - Detect imx800 sensor
 * @imx800: pointer to imx800 device
 *
 * Return: 0 if successful, -EIO if sensor id does not match
 */
static int imx800_detect(struct imx800 *imx800)
{
	int ret;
	u32 val;

	ret = imx800_read_reg(imx800, IMX800_REG_ID, 2, &val);
	if (ret)
		return ret;

	if (val != IMX800_ID) {
		dev_err(imx800->dev, "chip id mismatch: %x!=%x",
			IMX800_ID, val);
		return -ENXIO;
	}

	return 0;
}

/**
 * imx800_parse_hw_config() - Parse HW configuration and check if supported
 * @imx800: pointer to imx800 device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx800_parse_hw_config(struct imx800 *imx800)
{
	struct fwnode_handle *fwnode = dev_fwnode(imx800->dev);
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
	imx800->reset_gpio = devm_gpiod_get_optional(imx800->dev, "reset",
						     GPIOD_OUT_LOW);
	if (IS_ERR(imx800->reset_gpio)) {
		dev_err(imx800->dev, "failed to get reset gpio %ld",
			PTR_ERR(imx800->reset_gpio));
		return PTR_ERR(imx800->reset_gpio);
	}

	/* Get sensor input clock */
	imx800->inclk = devm_clk_get(imx800->dev, NULL);
	if (IS_ERR(imx800->inclk)) {
		dev_err(imx800->dev, "could not get inclk");
		return PTR_ERR(imx800->inclk);
	}

	rate = clk_get_rate(imx800->inclk);
	if (rate != IMX800_INCLK_RATE) {
		dev_err(imx800->dev, "inclk frequency mismatch");
		return -EINVAL;
	}

	/* Get optional DT defined regulators */
	for (i = 0; i < ARRAY_SIZE(imx800_supply_names); i++)
		imx800->supplies[i].supply = imx800_supply_names[i];

	ret = devm_regulator_bulk_get(imx800->dev,
				      ARRAY_SIZE(imx800_supply_names),
				      imx800->supplies);
	if (ret)
		return ret;

	ep = fwnode_graph_get_next_endpoint(fwnode, NULL);
	if (!ep)
		return -ENXIO;

	ret = v4l2_fwnode_endpoint_alloc_parse(ep, &bus_cfg);
	fwnode_handle_put(ep);
	if (ret)
		return ret;

	if (bus_cfg.bus.mipi_csi2.num_data_lanes != IMX800_NUM_DATA_LANES) {
		dev_err(imx800->dev,
			"number of CSI2 data lanes %d is not supported",
			bus_cfg.bus.mipi_csi2.num_data_lanes);
		ret = -EINVAL;
		goto done_endpoint_free;
	}

	if (!bus_cfg.nr_of_link_frequencies) {
		dev_err(imx800->dev, "no link frequencies defined");
		ret = -EINVAL;
		goto done_endpoint_free;
	}

	for (i = 0; i < bus_cfg.nr_of_link_frequencies; i++)
		if (bus_cfg.link_frequencies[i] == IMX800_LINK_FREQ)
			goto done_endpoint_free;

	ret = -EINVAL;

done_endpoint_free:
	v4l2_fwnode_endpoint_free(&bus_cfg);

	return ret;
}

/* V4l2 subdevice ops */
static const struct v4l2_subdev_video_ops imx800_video_ops = {
	.s_stream = imx800_set_stream,
};

static const struct v4l2_subdev_pad_ops imx800_pad_ops = {
	.init_cfg = imx800_init_pad_cfg,
	.enum_mbus_code = imx800_enum_mbus_code,
	.enum_frame_size = imx800_enum_frame_size,
	.get_fmt = imx800_get_pad_format,
	.set_fmt = imx800_set_pad_format,
};

static const struct v4l2_subdev_ops imx800_subdev_ops = {
	.video = &imx800_video_ops,
	.pad = &imx800_pad_ops,
};

/**
 * imx800_power_on() - Sensor power on sequence
 * @dev: pointer to i2c device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx800_power_on(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct imx800 *imx800 = to_imx800(sd);
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(imx800_supply_names),
				    imx800->supplies);
	if (ret < 0) {
		dev_err(dev, "failed to enable regulators\n");
		return ret;
	}

	gpiod_set_value_cansleep(imx800->reset_gpio, 0);

	ret = clk_prepare_enable(imx800->inclk);
	if (ret) {
		dev_err(imx800->dev, "fail to enable inclk");
		goto error_reset;
	}

	usleep_range(1000, 1200);

	return 0;

error_reset:
	gpiod_set_value_cansleep(imx800->reset_gpio, 1);
	regulator_bulk_disable(ARRAY_SIZE(imx800_supply_names),
			       imx800->supplies);

	return ret;
}

/**
 * imx800_power_off() - Sensor power off sequence
 * @dev: pointer to i2c device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx800_power_off(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct imx800 *imx800 = to_imx800(sd);

	clk_disable_unprepare(imx800->inclk);

	gpiod_set_value_cansleep(imx800->reset_gpio, 1);

	regulator_bulk_disable(ARRAY_SIZE(imx800_supply_names),
			       imx800->supplies);

	return 0;
}

/**
 * imx800_init_controls() - Initialize sensor subdevice controls
 * @imx800: pointer to imx800 device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx800_init_controls(struct imx800 *imx800)
{
	struct v4l2_ctrl_handler *ctrl_hdlr = &imx800->ctrl_handler;
	const struct imx800_mode *mode = imx800->cur_mode;
	u32 lpfr;
	int ret;

	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 6);
	if (ret)
		return ret;

	/* Serialize controls with sensor device */
	ctrl_hdlr->lock = &imx800->mutex;

	/* Initialize exposure and gain */
	lpfr = mode->vblank + mode->height;
	imx800->exp_ctrl = v4l2_ctrl_new_std(ctrl_hdlr,
					     &imx800_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     IMX800_EXPOSURE_MIN,
					     lpfr - IMX800_EXPOSURE_OFFSET,
					     IMX800_EXPOSURE_STEP,
					     IMX800_EXPOSURE_DEFAULT);

	imx800->again_ctrl = v4l2_ctrl_new_std(ctrl_hdlr,
					       &imx800_ctrl_ops,
					       V4L2_CID_ANALOGUE_GAIN,
					       IMX800_AGAIN_MIN,
					       IMX800_AGAIN_MAX,
					       IMX800_AGAIN_STEP,
					       IMX800_AGAIN_DEFAULT);

	v4l2_ctrl_cluster(2, &imx800->exp_ctrl);

	imx800->vblank_ctrl = v4l2_ctrl_new_std(ctrl_hdlr,
						&imx800_ctrl_ops,
						V4L2_CID_VBLANK,
						mode->vblank_min,
						mode->vblank_max,
						1, mode->vblank);

	/* Read only controls */
	imx800->pclk_ctrl = v4l2_ctrl_new_std(ctrl_hdlr,
					      &imx800_ctrl_ops,
					      V4L2_CID_PIXEL_RATE,
					      mode->pclk, mode->pclk,
					      1, mode->pclk);

	imx800->link_freq_ctrl = v4l2_ctrl_new_int_menu(ctrl_hdlr,
							&imx800_ctrl_ops,
							V4L2_CID_LINK_FREQ,
							ARRAY_SIZE(link_freq) -
							1,
							mode->link_freq_idx,
							link_freq);
	if (imx800->link_freq_ctrl)
		imx800->link_freq_ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	imx800->hblank_ctrl = v4l2_ctrl_new_std(ctrl_hdlr,
						&imx800_ctrl_ops,
						V4L2_CID_HBLANK,
						IMX800_REG_MIN,
						IMX800_REG_MAX,
						1, mode->hblank);
	if (imx800->hblank_ctrl)
		imx800->hblank_ctrl->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	if (ctrl_hdlr->error) {
		dev_err(imx800->dev, "control init failed: %d",
			ctrl_hdlr->error);
		v4l2_ctrl_handler_free(ctrl_hdlr);
		return ctrl_hdlr->error;
	}

	imx800->sd.ctrl_handler = ctrl_hdlr;

	return 0;
}

/**
 * imx800_probe() - I2C client device binding
 * @client: pointer to i2c client device
 *
 * Return: 0 if successful, error code otherwise.
 */
static int imx800_probe(struct i2c_client *client)
{
	struct imx800 *imx800;
	const char *name;
	int ret;

	pr_info("imx800_probe start\n");
	imx800 = devm_kzalloc(&client->dev, sizeof(*imx800), GFP_KERNEL);
	if (!imx800)
		return -ENOMEM;

	imx800->dev = &client->dev;
	name = device_get_match_data(&client->dev);
	if (!name)
		return -ENODEV;

	dev_info(imx800->dev, "imx800_probe 1\n");
	/* Initialize subdev */
	v4l2_i2c_subdev_init(&imx800->sd, client, &imx800_subdev_ops);

	ret = imx800_parse_hw_config(imx800);
	if (ret) {
		dev_err(imx800->dev, "HW configuration is not supported");
		return ret;
	}

	dev_info(imx800->dev, "imx800_probe 2\n");
	mutex_init(&imx800->mutex);

	ret = imx800_power_on(imx800->dev);
	if (ret) {
		dev_err(imx800->dev, "failed to power-on the sensor");
		goto error_mutex_destroy;
	}

	dev_info(imx800->dev, "imx800_probe 3\n");
	/* Check module identity */
	ret = imx800_detect(imx800);
	if (ret) {
		dev_err(imx800->dev, "failed to find sensor: %d", ret);
		goto error_power_off;
	}

	dev_info(imx800->dev, "imx800_probe 4\n");
	/* Set default mode to max resolution */
	imx800->cur_mode = &supported_mode;
	imx800->vblank = imx800->cur_mode->vblank;

	ret = imx800_init_controls(imx800);
	if (ret) {
		dev_err(imx800->dev, "failed to init controls: %d", ret);
		goto error_power_off;
	}

	dev_info(imx800->dev, "imx800_probe 5\n");
	/* Initialize subdev */
	imx800->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	imx800->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	v4l2_i2c_subdev_set_name(&imx800->sd, client, name, NULL);

	dev_info(imx800->dev, "imx800_probe 6\n");
	/* Initialize source pad */
	imx800->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_pads_init(&imx800->sd.entity, 1, &imx800->pad);
	if (ret) {
		dev_err(imx800->dev, "failed to init entity pads: %d", ret);
		goto error_handler_free;
	}

	dev_info(imx800->dev, "imx800_probe 7\n");
	ret = v4l2_async_register_subdev_sensor(&imx800->sd);
	if (ret < 0) {
		dev_err(imx800->dev,
			"failed to register async subdev: %d", ret);
		goto error_media_entity;
	}

	dev_info(imx800->dev, "imx800_probe 8\n");
	pm_runtime_set_active(imx800->dev);
	pm_runtime_enable(imx800->dev);
	pm_runtime_idle(imx800->dev);
	dev_info(imx800->dev, "imx800_probe end\n");

	return 0;

error_media_entity:
	media_entity_cleanup(&imx800->sd.entity);
error_handler_free:
	v4l2_ctrl_handler_free(imx800->sd.ctrl_handler);
error_power_off:
	imx800_power_off(imx800->dev);
error_mutex_destroy:
	mutex_destroy(&imx800->mutex);

	return ret;
}

/**
 * imx800_remove() - I2C client device unbinding
 * @client: pointer to I2C client device
 *
 * Return: 0 if successful, error code otherwise.
 */
static void imx800_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx800 *imx800 = to_imx800(sd);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(sd->ctrl_handler);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		imx800_power_off(&client->dev);
	pm_runtime_set_suspended(&client->dev);

	mutex_destroy(&imx800->mutex);
}

static const struct dev_pm_ops imx800_pm_ops = {
	SET_RUNTIME_PM_OPS(imx800_power_off, imx800_power_on, NULL)
};

static const struct of_device_id imx800_of_match[] = {
	{ .compatible = "sony,imx800", .data = "imx800" },
	{ }
};

MODULE_DEVICE_TABLE(of, imx800_of_match);

static struct i2c_driver imx800_driver = {
	.probe = imx800_probe,
	.remove = imx800_remove,
	.driver = {
		.name = "imx800",
		.pm = &imx800_pm_ops,
		.of_match_table = imx800_of_match,
	},
};

module_i2c_driver(imx800_driver);

MODULE_DESCRIPTION("Sony IMX800 sensor driver");
MODULE_LICENSE("GPL");
