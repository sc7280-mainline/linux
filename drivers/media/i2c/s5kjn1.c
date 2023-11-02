// SPDX-License-Identifier: GPL-2.0
/*
 * A V4L2 driver for Sony S5KJN1 cameras.
 * Copyright (C) 2023 Matti Lehtimäki
 *
 * Based on Sony imx219 camera driver
 * Copyright (C) 2019 Raspberry Pi (Trading) Ltd
 * Copyright (C) 2018 Intel Corporation
 * Copyright (C) 2018 Qtechnology A/S
 *
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>

#include <media/v4l2-cci.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-mediabus.h>

/* Chip ID */
#define S5KJN1_REG_CHIP_ID		CCI_REG16(0x0000)
#define S5KJN1_CHIP_ID			0x38e1

#define S5KJN1_REG_MODE_SELECT		CCI_REG8(0x0100)
#define S5KJN1_MODE_STANDBY		0x00
#define S5KJN1_MODE_STREAMING		0x01

/* Group hold register */
#define S5KJN1_REG_HOLD			CCI_REG8(0x0104)
#define S5KJN1_HOLD_DISABLE		0x00
#define S5KJN1_HOLD_ENABLE		0x01

/* Analog gain control */
#define S5KJN1_REG_ANALOG_GAIN		CCI_REG8(0x0204)
#define S5KJN1_ANA_GAIN_MIN		0x20
#define S5KJN1_ANA_GAIN_MAX		0x200
#define S5KJN1_ANA_GAIN_DEFAULT		0xc0
#define S5KJN1_ANA_GAIN_STEP		1

/* Digital gain control */
#define S5KJN1_REG_DIGITAL_GAIN		CCI_REG16(0x020e)
#define S5KJN1_DGTL_GAIN_MIN		0x0100
#define S5KJN1_DGTL_GAIN_MAX		0x0fff
#define S5KJN1_DGTL_GAIN_DEFAULT	0x0100
#define S5KJN1_DGTL_GAIN_STEP		1

/* Exposure control */
#define S5KJN1_REG_EXPOSURE		CCI_REG16(0x0202)
#define S5KJN1_EXPOSURE_MIN		0
#define S5KJN1_EXPOSURE_MAX		0xffcc
#define S5KJN1_EXPOSURE_STEP		1
#define S5KJN1_EXPOSURE_DEFAULT		0x0f00

/* V_TIMING internal */
#define S5KJN1_REG_VTS			CCI_REG16(0x0340)
#define S5KJN1_VTS_MAX			0xffff

#define S5KJN1_VBLANK_MIN		4

/* HBLANK control - read only */
#define S5KJN1_PPL_DEFAULT		3448

#define S5KJN1_REG_ORIENTATION		CCI_REG8(0x0101)

/* Test Pattern Control */
#define S5KJN1_REG_TEST_PATTERN		CCI_REG16(0x0600)
#define S5KJN1_TEST_PATTERN_DISABLE	0
#define S5KJN1_TEST_PATTERN_SOLID_COLOR	1
#define S5KJN1_TEST_PATTERN_COLOR_BARS	2
#define S5KJN1_TEST_PATTERN_GREY_COLOR	3
#define S5KJN1_TEST_PATTERN_PN9		4

#define S5KJN1_REG_TP_WINDOW_WIDTH	CCI_REG16(0x0624)
#define S5KJN1_REG_TP_WINDOW_HEIGHT	CCI_REG16(0x0626)

/* External clock frequency is 24.0M */
#define S5KJN1_XCLK_FREQ		24000000

//TODO
/* Pixel rate is fixed for all the modes */
#define S5KJN1_PIXEL_RATE		1176690000


#define S5KJN1_DEFAULT_LINK_FREQ	828000000

/* S5KJN1 native and active pixel array size. */
#define S5KJN1_NATIVE_WIDTH		4080U
#define S5KJN1_NATIVE_HEIGHT		3072U
#define S5KJN1_PIXEL_ARRAY_LEFT		9U
#define S5KJN1_PIXEL_ARRAY_TOP		0U
#define S5KJN1_PIXEL_ARRAY_WIDTH	4064U
#define S5KJN1_PIXEL_ARRAY_HEIGHT	3072U

struct s5kjn1_reg_list {
	unsigned int num_of_regs;
	const struct cci_reg_sequence *regs;
};

/* Mode : resolution and related config&values */
struct s5kjn1_mode {
	/* Frame width */
	unsigned int width;
	/* Frame height */
	unsigned int height;

	/* V-timing */
	unsigned int vts_def;

	/* Default register values */
	struct s5kjn1_reg_list reg_list;
};

static const struct cci_reg_sequence mode_4080x3072_regs[] = {
	{ CCI_REG16(0x6028), 0x2400 }, //Global, Analog setting
	{ CCI_REG16(0x602A), 0x1354 },
	{ CCI_REG16(0x6F12), 0x0100 },
	{ CCI_REG16(0x6F12), 0x7017 },
	{ CCI_REG16(0x602A), 0x13B2 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x602A), 0x1236 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x602A), 0x1A0A },
	{ CCI_REG16(0x6F12), 0x4C0A },
	{ CCI_REG16(0x602A), 0x2210 },
	{ CCI_REG16(0x6F12), 0x3401 },
	{ CCI_REG16(0x602A), 0x2176 },
	{ CCI_REG16(0x6F12), 0x6400 },
	{ CCI_REG16(0x602A), 0x222E },
	{ CCI_REG16(0x6F12), 0x0001 },
	{ CCI_REG16(0x602A), 0x06B6 },
	{ CCI_REG16(0x6F12), 0x0A00 },
	{ CCI_REG16(0x602A), 0x06BC },
	{ CCI_REG16(0x6F12), 0x1001 },
	{ CCI_REG16(0x602A), 0x2140 },
	{ CCI_REG16(0x6F12), 0x0101 },
	{ CCI_REG16(0x602A), 0x1A0E },
	{ CCI_REG16(0x6F12), 0x9600 },
	{ CCI_REG16(0x6028), 0x4000 },
	{ CCI_REG16(0xF44E), 0x0011 },
	{ CCI_REG16(0xF44C), 0x0B0B },
	{ CCI_REG16(0xF44A), 0x0006 },
	{ CCI_REG16(0x0118), 0x0002 },
	{ CCI_REG16(0x011A), 0x0001 },

	{ CCI_REG16(0x6028), 0x2400 }, // Mode setting
	{ CCI_REG16(0x602A), 0x1A28 },
	{ CCI_REG16(0x6F12), 0x4C00 },
	{ CCI_REG16(0x602A), 0x065A },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x602A), 0x139E },
	{ CCI_REG16(0x6F12), 0x0100 },
	{ CCI_REG16(0x602A), 0x139C },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x602A), 0x13A0 },
	{ CCI_REG16(0x6F12), 0x0A00 },
	{ CCI_REG16(0x6F12), 0x0120 },
	{ CCI_REG16(0x602A), 0x2072 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x602A), 0x1A64 },
	{ CCI_REG16(0x6F12), 0x0301 },
	{ CCI_REG16(0x6F12), 0xFF00 },
	{ CCI_REG16(0x602A), 0x19E6 },
	{ CCI_REG16(0x6F12), 0x0200 },
	{ CCI_REG16(0x602A), 0x1A30 },
	{ CCI_REG16(0x6F12), 0x3401 },
	{ CCI_REG16(0x602A), 0x19FC },
	{ CCI_REG16(0x6F12), 0x0B00 },
	{ CCI_REG16(0x602A), 0x19F4 },
	{ CCI_REG16(0x6F12), 0x0606 },
	{ CCI_REG16(0x602A), 0x19F8 },
	{ CCI_REG16(0x6F12), 0x1010 },
	{ CCI_REG16(0x602A), 0x1B26 },
	{ CCI_REG16(0x6F12), 0x6F80 },
	{ CCI_REG16(0x6F12), 0xA060 },
	{ CCI_REG16(0x602A), 0x1A3C },
	{ CCI_REG16(0x6F12), 0x6207 },
	{ CCI_REG16(0x602A), 0x1A48 },
	{ CCI_REG16(0x6F12), 0x6207 },
	{ CCI_REG16(0x602A), 0x1444 },
	{ CCI_REG16(0x6F12), 0x2000 },
	{ CCI_REG16(0x6F12), 0x2000 },
	{ CCI_REG16(0x602A), 0x144C },
	{ CCI_REG16(0x6F12), 0x3F00 },
	{ CCI_REG16(0x6F12), 0x3F00 },
	{ CCI_REG16(0x602A), 0x7F6C },
	{ CCI_REG16(0x6F12), 0x0100 },
	{ CCI_REG16(0x6F12), 0x2F00 },
	{ CCI_REG16(0x6F12), 0xFA00 },
	{ CCI_REG16(0x6F12), 0x2400 },
	{ CCI_REG16(0x6F12), 0xE500 },
	{ CCI_REG16(0x602A), 0x0650 },
	{ CCI_REG16(0x6F12), 0x0600 },
	{ CCI_REG16(0x602A), 0x0654 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x602A), 0x1A46 },
	{ CCI_REG16(0x6F12), 0x8A00 },
	{ CCI_REG16(0x602A), 0x1A52 },
	{ CCI_REG16(0x6F12), 0xBF00 },
	{ CCI_REG16(0x602A), 0x0674 },
	{ CCI_REG16(0x6F12), 0x0500 },
	{ CCI_REG16(0x6F12), 0x0500 },
	{ CCI_REG16(0x6F12), 0x0500 },
	{ CCI_REG16(0x6F12), 0x0500 },
	{ CCI_REG16(0x602A), 0x0668 },
	{ CCI_REG16(0x6F12), 0x0800 },
	{ CCI_REG16(0x6F12), 0x0800 },
	{ CCI_REG16(0x6F12), 0x0800 },
	{ CCI_REG16(0x6F12), 0x0800 },
	{ CCI_REG16(0x602A), 0x0684 },
	{ CCI_REG16(0x6F12), 0x4001 },
	{ CCI_REG16(0x602A), 0x0688 },
	{ CCI_REG16(0x6F12), 0x4001 },
	{ CCI_REG16(0x602A), 0x147C },
	{ CCI_REG16(0x6F12), 0x1000 },
	{ CCI_REG16(0x602A), 0x1480 },
	{ CCI_REG16(0x6F12), 0x1000 },
	{ CCI_REG16(0x602A), 0x19F6 },
	{ CCI_REG16(0x6F12), 0x0904 },
	{ CCI_REG16(0x602A), 0x0812 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x602A), 0x1A02 },
	{ CCI_REG16(0x6F12), 0x1800 },
	{ CCI_REG16(0x602A), 0x2148 },
	{ CCI_REG16(0x6F12), 0x0100 },
	{ CCI_REG16(0x602A), 0x2042 },
	{ CCI_REG16(0x6F12), 0x1A00 },
	{ CCI_REG16(0x602A), 0x0874 },
	{ CCI_REG16(0x6F12), 0x0100 },
	{ CCI_REG16(0x602A), 0x09C0 },
	{ CCI_REG16(0x6F12), 0x2008 },
	{ CCI_REG16(0x602A), 0x09C4 },
	{ CCI_REG16(0x6F12), 0x2000 },
	{ CCI_REG16(0x602A), 0x19FE },
	{ CCI_REG16(0x6F12), 0x0E1C },
	{ CCI_REG16(0x602A), 0x4D92 },
	{ CCI_REG16(0x6F12), 0x0100 },
	{ CCI_REG16(0x602A), 0x84C8 },
	{ CCI_REG16(0x6F12), 0x0100 },
	{ CCI_REG16(0x602A), 0x4D94 },
	{ CCI_REG16(0x6F12), 0x0005 },
	{ CCI_REG16(0x6F12), 0x000A },
	{ CCI_REG16(0x6F12), 0x0010 },
	{ CCI_REG16(0x6F12), 0x0810 },
	{ CCI_REG16(0x6F12), 0x000A },
	{ CCI_REG16(0x6F12), 0x0040 },
	{ CCI_REG16(0x6F12), 0x0810 },
	{ CCI_REG16(0x6F12), 0x0810 },
	{ CCI_REG16(0x6F12), 0x8002 },
	{ CCI_REG16(0x6F12), 0xFD03 },
	{ CCI_REG16(0x6F12), 0x0010 },
	{ CCI_REG16(0x6F12), 0x1510 },
	{ CCI_REG16(0x602A), 0x3570 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x602A), 0x3574 },
	{ CCI_REG16(0x6F12), 0x1201 },
	{ CCI_REG16(0x602A), 0x21E4 },
	{ CCI_REG16(0x6F12), 0x0400 },
	{ CCI_REG16(0x602A), 0x21EC },
	{ CCI_REG16(0x6F12), 0x1F04 },
	{ CCI_REG16(0x602A), 0x2080 },
	{ CCI_REG16(0x6F12), 0x0101 },
	{ CCI_REG16(0x6F12), 0xFF00 },
	{ CCI_REG16(0x6F12), 0x7F01 },
	{ CCI_REG16(0x6F12), 0x0001 },
	{ CCI_REG16(0x6F12), 0x8001 },
	{ CCI_REG16(0x6F12), 0xD244 },
	{ CCI_REG16(0x6F12), 0xD244 },
	{ CCI_REG16(0x6F12), 0x14F4 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x602A), 0x20BA },
	{ CCI_REG16(0x6F12), 0x141C },
	{ CCI_REG16(0x6F12), 0x111C },
	{ CCI_REG16(0x6F12), 0x54F4 },
	{ CCI_REG16(0x602A), 0x120E },
	{ CCI_REG16(0x6F12), 0x1000 },
	{ CCI_REG16(0x602A), 0x212E },
	{ CCI_REG16(0x6F12), 0x0200 },
	{ CCI_REG16(0x602A), 0x13AE },
	{ CCI_REG16(0x6F12), 0x0101 },
	{ CCI_REG16(0x602A), 0x0718 },
	{ CCI_REG16(0x6F12), 0x0001 },
	{ CCI_REG16(0x602A), 0x0710 },
	{ CCI_REG16(0x6F12), 0x0002 },
	{ CCI_REG16(0x6F12), 0x0804 },
	{ CCI_REG16(0x6F12), 0x0100 },
	{ CCI_REG16(0x602A), 0x1B5C },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x602A), 0x0786 },
	{ CCI_REG16(0x6F12), 0x7701 },
	{ CCI_REG16(0x602A), 0x2022 },
	{ CCI_REG16(0x6F12), 0x0500 },
	{ CCI_REG16(0x6F12), 0x0500 },
	{ CCI_REG16(0x602A), 0x1360 },
	{ CCI_REG16(0x6F12), 0x0100 },
	{ CCI_REG16(0x602A), 0x1376 },
	{ CCI_REG16(0x6F12), 0x0100 },
	{ CCI_REG16(0x6F12), 0x6038 },
	{ CCI_REG16(0x6F12), 0x7038 },
	{ CCI_REG16(0x6F12), 0x8038 },
	{ CCI_REG16(0x602A), 0x1386 },
	{ CCI_REG16(0x6F12), 0x0B00 },
	{ CCI_REG16(0x602A), 0x06FA },
	{ CCI_REG16(0x6F12), 0x1000 },
	{ CCI_REG16(0x602A), 0x4A94 },
	{ CCI_REG16(0x6F12), 0x0900 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0300 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0300 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0900 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x602A), 0x0A76 },
	{ CCI_REG16(0x6F12), 0x1000 },
	{ CCI_REG16(0x602A), 0x0AEE },
	{ CCI_REG16(0x6F12), 0x1000 },
	{ CCI_REG16(0x602A), 0x0B66 },
	{ CCI_REG16(0x6F12), 0x1000 },
	{ CCI_REG16(0x602A), 0x0BDE },
	{ CCI_REG16(0x6F12), 0x1000 },
	{ CCI_REG16(0x602A), 0x0BE8 },
	{ CCI_REG16(0x6F12), 0x3000 },
	{ CCI_REG16(0x6F12), 0x3000 },
	{ CCI_REG16(0x602A), 0x0C56 },
	{ CCI_REG16(0x6F12), 0x1000 },
	{ CCI_REG16(0x602A), 0x0C60 },
	{ CCI_REG16(0x6F12), 0x3000 },
	{ CCI_REG16(0x6F12), 0x3000 },
	{ CCI_REG16(0x602A), 0x0CB6 },
	{ CCI_REG16(0x6F12), 0x0100 },
	{ CCI_REG16(0x602A), 0x0CF2 },
	{ CCI_REG16(0x6F12), 0x0001 },
	{ CCI_REG16(0x602A), 0x0CF0 },
	{ CCI_REG16(0x6F12), 0x0101 },
	{ CCI_REG16(0x602A), 0x11B8 },
	{ CCI_REG16(0x6F12), 0x0100 },
	{ CCI_REG16(0x602A), 0x11F6 },
	{ CCI_REG16(0x6F12), 0x0020 },
	{ CCI_REG16(0x602A), 0x4A74 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0xD8FF },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0xD8FF },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x602A), 0x218E },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x602A), 0x2268 },
	{ CCI_REG16(0x6F12), 0xF279 },
	{ CCI_REG16(0x602A), 0x5006 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x602A), 0x500E },
	{ CCI_REG16(0x6F12), 0x0100 },
	{ CCI_REG16(0x602A), 0x4E70 },
	{ CCI_REG16(0x6F12), 0x2062 },
	{ CCI_REG16(0x6F12), 0x5501 },
	{ CCI_REG16(0x602A), 0x06DC },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6F12), 0x0000 },
	{ CCI_REG16(0x6028), 0x4000 },
	{ CCI_REG16(0xF46A), 0xAE80 },
	{ CCI_REG16(0x0344), 0x0000 }, //x_addr_start
	{ CCI_REG16(0x0346), 0x0000 }, //y_addr_start
	{ CCI_REG16(0x0348), 0x1FFF }, //x_addr_end
	{ CCI_REG16(0x034A), 0x181F }, //y_addr_end
	{ CCI_REG16(0x034C), 0x0FF0 }, //output width
	{ CCI_REG16(0x034E), 0x0C00 }, //output height
	{ CCI_REG16(0x0350), 0x0008 },
	{ CCI_REG16(0x0352), 0x0008 },
	{ CCI_REG16(0x0900), 0x0122 },
	{ CCI_REG16(0x0380), 0x0002 },
	{ CCI_REG16(0x0382), 0x0002 },
	{ CCI_REG16(0x0384), 0x0002 },
	{ CCI_REG16(0x0386), 0x0002 },
	{ CCI_REG16(0x0110), 0x1002 },
	{ CCI_REG16(0x0114), 0x0301 },
	{ CCI_REG16(0x0116), 0x3000 },
	{ CCI_REG16(0x0136), 0x1800 },
	{ CCI_REG16(0x013E), 0x0000 },
	{ CCI_REG16(0x0300), 0x0006 },
	{ CCI_REG16(0x0302), 0x0001 },
	{ CCI_REG16(0x0304), 0x0004 },
	{ CCI_REG16(0x0306), 0x008C },
	{ CCI_REG16(0x0308), 0x0008 },
	{ CCI_REG16(0x030A), 0x0001 },
	{ CCI_REG16(0x030C), 0x0000 },
	{ CCI_REG16(0x030E), 0x0004 },
	{ CCI_REG16(0x0310), 0x008A },
	{ CCI_REG16(0x0312), 0x0000 },
	{ CCI_REG16(0x080E), 0x0000 },
	{ CCI_REG16(0x0340), 0x0FD6 },
	{ CCI_REG16(0x0342), 0x11E8 },
	{ CCI_REG16(0x0702), 0x0000 },
	{ CCI_REG16(0x0202), 0x0f00 },
	{ CCI_REG16(0x0200), 0x0100 },
	{ CCI_REG16(0x0D00), 0x0101 },
	{ CCI_REG16(0x0D02), 0x0101 },
	{ CCI_REG16(0x0D04), 0x0102 },
	{ CCI_REG16(0x6226), 0x0000 },

	//{0x0100, 0x0100},	// Streaming on
	{ CCI_REG16(0xffff), 0x00 },
};

static const s64 s5kjn1_link_freq_menu[] = {
	S5KJN1_DEFAULT_LINK_FREQ
};

static const char * const s5kjn1_test_pattern_menu[] = {
	"Disabled",
	"Color Bars",
	"Solid Color",
	"Grey Color Bars",
	"PN9"
};

static const int s5kjn1_test_pattern_val[] = {
	S5KJN1_TEST_PATTERN_DISABLE,
	S5KJN1_TEST_PATTERN_COLOR_BARS,
	S5KJN1_TEST_PATTERN_SOLID_COLOR,
	S5KJN1_TEST_PATTERN_GREY_COLOR,
	S5KJN1_TEST_PATTERN_PN9,
};

/* regulator supplies */
static const char * const s5kjn1_supply_name[] = {
	/* Supplies can be enabled in any order */
	"dovdd", /* Digital I/O power */
	"dvdd", /* Digital core power */
	"avdd", /* Analog power */
};

#define S5KJN1_NUM_SUPPLIES ARRAY_SIZE(s5kjn1_supply_name)

/*
 * The supported formats.
 * This table MUST contain 4 entries per format, to cover the various flip
 * combinations in the order
 * - no flip
 * - h flip
 * - v flip
 * - h&v flips
 */
static const u32 s5kjn1_mbus_formats[] = {
	MEDIA_BUS_FMT_SRGGB10_1X10,
	MEDIA_BUS_FMT_SGRBG10_1X10,
	MEDIA_BUS_FMT_SGBRG10_1X10,
	MEDIA_BUS_FMT_SBGGR10_1X10,

	MEDIA_BUS_FMT_SRGGB8_1X8,
	MEDIA_BUS_FMT_SGRBG8_1X8,
	MEDIA_BUS_FMT_SGBRG8_1X8,
	MEDIA_BUS_FMT_SBGGR8_1X8,
};

/*
 * Initialisation delay between XCLR low->high and the moment when the sensor
 * can start capture (i.e. can leave software stanby) must be not less than:
 *   t4 + max(t5, t6 + <time to initialize the sensor register over I2C>)
 * where
 *   t4 is fixed, and is max 200uS,
 *   t5 is fixed, and is 6000uS,
 *   t6 depends on the sensor external clock, and is max 32000 clock periods.
 * As per sensor datasheet, the external clock must be from 6MHz to 27MHz.
 * So for any acceptable external clock t6 is always within the range of
 * 1185 to 5333 uS, and is always less than t5.
 * For this reason this is always safe to wait (t4 + t5) = 6200 uS, then
 * initialize the sensor over I2C, and then exit the software standby.
 *
 * This start-up time can be optimized a bit more, if we start the writes
 * over I2C after (t4+t6), but before (t4+t5) expires. But then sensor
 * initialization over I2C may complete before (t4+t5) expires, and we must
 * ensure that capture is not started before (t4+t5).
 *
 * This delay doesn't account for the power supply startup time. If needed,
 * this should be taken care of via the regulator framework. E.g. in the
 * case of DT for regulator-fixed one should define the startup-delay-us
 * property.
 */
#define S5KJN1_XCLR_MIN_DELAY_US	6200
#define S5KJN1_XCLR_DELAY_RANGE_US	1000

/* Mode configs */
static const struct s5kjn1_mode supported_modes[] = {
	{
		.width = 4080,
		.height = 3072,
		.vts_def = 0x0fd6,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_4080x3072_regs),
			.regs = mode_4080x3072_regs,
		},
	},
};

struct s5kjn1 {
	struct v4l2_subdev sd;
	struct media_pad pad;

	struct regmap *regmap;
	struct clk *xclk; /* system clock to S5KJN1 */
	u32 xclk_freq;

	struct gpio_desc *pwdn_gpio;
	struct gpio_desc *reset_gpio;
	struct regulator_bulk_data supplies[S5KJN1_NUM_SUPPLIES];

	struct v4l2_ctrl_handler ctrl_handler;
	/* V4L2 Controls */
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *exposure;
	struct v4l2_ctrl *vflip;
	struct v4l2_ctrl *hflip;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *hblank;

	/* Current mode */
	const struct s5kjn1_mode *mode;
};

static inline struct s5kjn1 *to_s5kjn1(struct v4l2_subdev *_sd)
{
	return container_of(_sd, struct s5kjn1, sd);
}

/* Get bayer order based on flip setting. */
static u32 s5kjn1_get_format_code(struct s5kjn1 *s5kjn1, u32 code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(s5kjn1_mbus_formats); i++)
		if (s5kjn1_mbus_formats[i] == code)
			break;

	if (i >= ARRAY_SIZE(s5kjn1_mbus_formats))
		i = 0;

	i = (i & ~3) | (s5kjn1->vflip->val ? 2 : 0) |
	    (s5kjn1->hflip->val ? 1 : 0);

	return s5kjn1_mbus_formats[i];
}

/* -----------------------------------------------------------------------------
 * Controls
 */

static int s5kjn1_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct s5kjn1 *s5kjn1 =
		container_of(ctrl->handler, struct s5kjn1, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&s5kjn1->sd);
	const struct v4l2_mbus_framefmt *format;
	struct v4l2_subdev_state *state;
	int ret = 0;

	state = v4l2_subdev_get_locked_active_state(&s5kjn1->sd);
	format = v4l2_subdev_get_pad_format(&s5kjn1->sd, state, 0);
	dev_info(&client->dev, "s5kjn1_set_ctrl %x\n", ctrl->id);

	if (ctrl->id == V4L2_CID_VBLANK) {
		int exposure_max, exposure_def;

		/* Update max exposure while meeting expected vblanking */
		exposure_max = format->height + ctrl->val - 4;
		exposure_def = (exposure_max < S5KJN1_EXPOSURE_DEFAULT) ?
			exposure_max : S5KJN1_EXPOSURE_DEFAULT;
		__v4l2_ctrl_modify_range(s5kjn1->exposure,
					 s5kjn1->exposure->minimum,
					 exposure_max, s5kjn1->exposure->step,
					 exposure_def);
	}

	/*
	 * Applying V4L2 control value only happens
	 * when power is up for streaming
	 */
	if (pm_runtime_get_if_in_use(&client->dev) == 0)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_ANALOGUE_GAIN:
		cci_write(s5kjn1->regmap, S5KJN1_REG_ANALOG_GAIN,
			  ctrl->val, &ret);
		break;
	case V4L2_CID_EXPOSURE:
		cci_write(s5kjn1->regmap, S5KJN1_REG_EXPOSURE,
			  ctrl->val, &ret);
		break;
	case V4L2_CID_DIGITAL_GAIN:
//		cci_write(s5kjn1->regmap, S5KJN1_REG_DIGITAL_GAIN,
//			  ctrl->val, &ret);
		break;
	case V4L2_CID_TEST_PATTERN:
		cci_write(s5kjn1->regmap, S5KJN1_REG_TEST_PATTERN,
			  s5kjn1_test_pattern_val[ctrl->val], &ret);
		break;
	case V4L2_CID_HFLIP:
	case V4L2_CID_VFLIP:
		cci_write(s5kjn1->regmap, S5KJN1_REG_ORIENTATION,
			  s5kjn1->hflip->val | s5kjn1->vflip->val << 1, &ret);
		break;
	case V4L2_CID_VBLANK:
//		cci_write(s5kjn1->regmap, S5KJN1_REG_VTS,
//			  format->height + ctrl->val, &ret);
		break;
	default:
		dev_info(&client->dev,
			 "ctrl(id:0x%x,val:0x%x) is not handled\n",
			 ctrl->id, ctrl->val);
		ret = -EINVAL;
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops s5kjn1_ctrl_ops = {
	.s_ctrl = s5kjn1_set_ctrl,
};

/* Initialize control handlers */
static int s5kjn1_init_controls(struct s5kjn1 *s5kjn1)
{
	struct i2c_client *client = v4l2_get_subdevdata(&s5kjn1->sd);
	const struct s5kjn1_mode *mode = &supported_modes[0];
	struct v4l2_ctrl_handler *ctrl_hdlr;
	struct v4l2_fwnode_device_properties props;
	int exposure_max, exposure_def, hblank;
	int ret;
	dev_err(&client->dev, "s5kjn1_init_controls\n");

	ctrl_hdlr = &s5kjn1->ctrl_handler;
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 12);
	if (ret)
		return ret;

	/* By default, PIXEL_RATE is read only */
	s5kjn1->pixel_rate = v4l2_ctrl_new_std(ctrl_hdlr, &s5kjn1_ctrl_ops,
					       V4L2_CID_PIXEL_RATE,
					       S5KJN1_PIXEL_RATE,
					       S5KJN1_PIXEL_RATE, 1,
					       S5KJN1_PIXEL_RATE);

	s5kjn1->link_freq =
		v4l2_ctrl_new_int_menu(ctrl_hdlr, &s5kjn1_ctrl_ops,
				       V4L2_CID_LINK_FREQ,
				       ARRAY_SIZE(s5kjn1_link_freq_menu) - 1, 0,
				       s5kjn1_link_freq_menu);
	if (s5kjn1->link_freq)
		s5kjn1->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	/* Initial vblank/hblank/exposure parameters based on current mode */
	s5kjn1->vblank = v4l2_ctrl_new_std(ctrl_hdlr, &s5kjn1_ctrl_ops,
					   V4L2_CID_VBLANK, S5KJN1_VBLANK_MIN,
					   S5KJN1_VTS_MAX - mode->height, 1,
					   mode->vts_def - mode->height);
	hblank = S5KJN1_PPL_DEFAULT - mode->width;
	s5kjn1->hblank = v4l2_ctrl_new_std(ctrl_hdlr, &s5kjn1_ctrl_ops,
					   V4L2_CID_HBLANK, hblank, hblank,
					   1, hblank);
	if (s5kjn1->hblank)
		s5kjn1->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	exposure_max = mode->vts_def - 4;
	exposure_def = (exposure_max < S5KJN1_EXPOSURE_DEFAULT) ?
		exposure_max : S5KJN1_EXPOSURE_DEFAULT;
	s5kjn1->exposure = v4l2_ctrl_new_std(ctrl_hdlr, &s5kjn1_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     S5KJN1_EXPOSURE_MIN, exposure_max,
					     S5KJN1_EXPOSURE_STEP,
					     exposure_def);

	v4l2_ctrl_new_std(ctrl_hdlr, &s5kjn1_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			  S5KJN1_ANA_GAIN_MIN, S5KJN1_ANA_GAIN_MAX,
			  S5KJN1_ANA_GAIN_STEP, S5KJN1_ANA_GAIN_DEFAULT);

	v4l2_ctrl_new_std(ctrl_hdlr, &s5kjn1_ctrl_ops, V4L2_CID_DIGITAL_GAIN,
			  S5KJN1_DGTL_GAIN_MIN, S5KJN1_DGTL_GAIN_MAX,
			  S5KJN1_DGTL_GAIN_STEP, S5KJN1_DGTL_GAIN_DEFAULT);

	s5kjn1->hflip = v4l2_ctrl_new_std(ctrl_hdlr, &s5kjn1_ctrl_ops,
					  V4L2_CID_HFLIP, 0, 1, 1, 0);
	if (s5kjn1->hflip)
		s5kjn1->hflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;

	s5kjn1->vflip = v4l2_ctrl_new_std(ctrl_hdlr, &s5kjn1_ctrl_ops,
					  V4L2_CID_VFLIP, 0, 1, 1, 0);
	if (s5kjn1->vflip)
		s5kjn1->vflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;

	v4l2_ctrl_new_std_menu_items(ctrl_hdlr, &s5kjn1_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(s5kjn1_test_pattern_menu) - 1,
				     0, 0, s5kjn1_test_pattern_menu);

	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&client->dev, "%s control init failed (%d)\n",
			__func__, ret);
		goto error;
	}

	ret = v4l2_fwnode_device_parse(&client->dev, &props);
	if (ret)
		goto error;

	ret = v4l2_ctrl_new_fwnode_properties(ctrl_hdlr, &s5kjn1_ctrl_ops,
					      &props);
	if (ret)
		goto error;

	s5kjn1->sd.ctrl_handler = ctrl_hdlr;
	dev_info(&client->dev, "s5kjn1_init_controls ok\n");

	return 0;

error:
	v4l2_ctrl_handler_free(ctrl_hdlr);
	dev_info(&client->dev, "s5kjn1_init_controls failed %i\n", ret);

	return ret;
}

static void s5kjn1_free_controls(struct s5kjn1 *s5kjn1)
{
	v4l2_ctrl_handler_free(s5kjn1->sd.ctrl_handler);
}

/* -----------------------------------------------------------------------------
 * Subdev operations
 */

static int s5kjn1_start_streaming(struct s5kjn1 *s5kjn1,
				  struct v4l2_subdev_state *state)
{
	struct i2c_client *client = v4l2_get_subdevdata(&s5kjn1->sd);
	const struct s5kjn1_reg_list *reg_list;
	int ret;
	dev_info(&client->dev, "s5kjn1_start_streaming\n");

	ret = pm_runtime_resume_and_get(&client->dev);
	if (ret < 0)
		return ret;

	/* Send all registers that are common to all modes */
/*
	ret = cci_multi_reg_write(s5kjn1->regmap, s5kjn1_common_regs,
				  ARRAY_SIZE(s5kjn1_common_regs), NULL);
	if (ret) {
		dev_err(&client->dev, "%s failed to send mfg header\n", __func__);
		goto err_rpm_put;
	}
*/
	ret = cci_write(s5kjn1->regmap, 0x6028, 0x4000, NULL);
	ret = cci_write(s5kjn1->regmap, 0x0000, 0x0003, NULL);
	ret = cci_write(s5kjn1->regmap, 0x0000, 0x38e1, NULL);
	ret = cci_write(s5kjn1->regmap, 0x001e, 0x0007, NULL);
	ret = cci_write(s5kjn1->regmap, 0x6028, 0x4000, NULL);
	ret = cci_write(s5kjn1->regmap, 0x6010, 0x0001, NULL);

	usleep_range(5000, 5100);

	ret = cci_write(s5kjn1->regmap, 0x6226, 0x0001, NULL);
	usleep_range(10000, 10100);
/*
	{ CCI_REG16(0x6028), 0x4000 }, // Page pointer HW
	{ CCI_REG16(0x0000), 0x0003 }, // Setfile Version
	{ CCI_REG16(0x0000), 0x38E1 }, // JN1( Sensor ID)
	{ CCI_REG16(0x001E), 0x0007 }, // V07

	{ CCI_REG16(0x6028), 0x4000 }, // Init setting
	{ CCI_REG16(0x6010), 0x0001 },
//	{ CCI_REG16(0xeeee), 5 }, //Delay 5ms
	{ CCI_REG16(0x6226), 0x0001 },
//	{ CCI_REG16(0xeeee), 10 }, //Delay 10ms
*/

	/* Apply default values of current mode */
	reg_list = &s5kjn1->mode->reg_list;
	ret = cci_multi_reg_write(s5kjn1->regmap, reg_list->regs,
				  reg_list->num_of_regs, NULL);
	if (ret) {
		dev_err(&client->dev, "%s failed to set mode\n", __func__);
		goto err_rpm_put;
	}

	/* Apply customized values from user */
	ret =  __v4l2_ctrl_handler_setup(s5kjn1->sd.ctrl_handler);
	if (ret)
		goto err_rpm_put;

	/* set stream on register */
	ret = cci_write(s5kjn1->regmap, S5KJN1_REG_MODE_SELECT,
			S5KJN1_MODE_STREAMING, NULL);
	if (ret)
		goto err_rpm_put;

	/* vflip and hflip cannot change during streaming */
	__v4l2_ctrl_grab(s5kjn1->vflip, true);
	__v4l2_ctrl_grab(s5kjn1->hflip, true);

	dev_info(&client->dev, "s5kjn1_start_streaming ok\n");
	return 0;

err_rpm_put:
	pm_runtime_put(&client->dev);
	dev_info(&client->dev, "s5kjn1_start_streaming failed %i\n", ret);
	return ret;
}

static void s5kjn1_stop_streaming(struct s5kjn1 *s5kjn1)
{
	struct i2c_client *client = v4l2_get_subdevdata(&s5kjn1->sd);
	int ret;
	dev_info(&client->dev, "s5kjn1_stop_streaming\n");

	/* set stream off register */
	ret = cci_write(s5kjn1->regmap, S5KJN1_REG_MODE_SELECT,
			S5KJN1_MODE_STANDBY, NULL);
	if (ret)
		dev_err(&client->dev, "%s failed to set stream\n", __func__);

	__v4l2_ctrl_grab(s5kjn1->vflip, false);
	__v4l2_ctrl_grab(s5kjn1->hflip, false);

	pm_runtime_put(&client->dev);
}

static int s5kjn1_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct s5kjn1 *s5kjn1 = to_s5kjn1(sd);
	struct v4l2_subdev_state *state;
	int ret = 0;
	dev_info(sd->dev, "s5kjn1_set_stream\n");

	state = v4l2_subdev_lock_and_get_active_state(sd);

	if (enable)
		ret = s5kjn1_start_streaming(s5kjn1, state);
	else
		s5kjn1_stop_streaming(s5kjn1);

	v4l2_subdev_unlock_state(state);
	return ret;
}

static void s5kjn1_update_pad_format(struct s5kjn1 *s5kjn1,
				     const struct s5kjn1_mode *mode,
				     struct v4l2_mbus_framefmt *fmt, u32 code)
{
	/* Bayer order varies with flips */
	fmt->code = s5kjn1_get_format_code(s5kjn1, code);
	fmt->width = mode->width;
	fmt->height = mode->height;
	fmt->field = V4L2_FIELD_NONE;
	fmt->colorspace = V4L2_COLORSPACE_RAW;
	fmt->ycbcr_enc = V4L2_YCBCR_ENC_601;
	fmt->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	fmt->xfer_func = V4L2_XFER_FUNC_NONE;
}

static int s5kjn1_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct s5kjn1 *s5kjn1 = to_s5kjn1(sd);
	dev_info(sd->dev, "s5kjn1_enum_mbus_code\n");

	if (code->index >= (ARRAY_SIZE(s5kjn1_mbus_formats) / 4))
		return -EINVAL;

	code->code = s5kjn1_get_format_code(s5kjn1, s5kjn1_mbus_formats[code->index * 4]);

	return 0;
}

static int s5kjn1_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *state,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	struct s5kjn1 *s5kjn1 = to_s5kjn1(sd);
	u32 code;
	dev_info(sd->dev, "s5kjn1_enum_frame_size\n");

	if (fse->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	code = s5kjn1_get_format_code(s5kjn1, fse->code);
	if (fse->code != code)
		return -EINVAL;

	fse->min_width = supported_modes[fse->index].width;
	fse->max_width = fse->min_width;
	fse->min_height = supported_modes[fse->index].height;
	fse->max_height = fse->min_height;

	return 0;
}

static int s5kjn1_set_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state,
				 struct v4l2_subdev_format *fmt)
{
	struct s5kjn1 *s5kjn1 = to_s5kjn1(sd);
	const struct s5kjn1_mode *mode;
	struct v4l2_mbus_framefmt *format;
	struct v4l2_rect *crop;
	unsigned int bin_h, bin_v;
	dev_info(sd->dev, "s5kjn1_set_pad_format\n");

	mode = v4l2_find_nearest_size(supported_modes,
				      ARRAY_SIZE(supported_modes),
				      width, height,
				      fmt->format.width, fmt->format.height);

	s5kjn1_update_pad_format(s5kjn1, mode, &fmt->format, fmt->format.code);

	format = v4l2_subdev_get_pad_format(sd, state, 0);
	*format = fmt->format;

	/*
	 * Use binning to maximize the crop rectangle size, and centre it in the
	 * sensor.
	 */
	bin_h = min(S5KJN1_PIXEL_ARRAY_WIDTH / format->width, 2U);
	bin_v = min(S5KJN1_PIXEL_ARRAY_HEIGHT / format->height, 2U);

	crop = v4l2_subdev_get_pad_crop(sd, state, 0);
	crop->width = format->width * bin_h;
	crop->height = format->height * bin_v;
	crop->left = (S5KJN1_NATIVE_WIDTH - crop->width) / 2;
	crop->top = (S5KJN1_NATIVE_HEIGHT - crop->height) / 2;

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		int exposure_max;
		int exposure_def;
		int hblank;

		s5kjn1->mode = mode;

		/* Update limits and set FPS to default */
		__v4l2_ctrl_modify_range(s5kjn1->vblank, S5KJN1_VBLANK_MIN,
					 S5KJN1_VTS_MAX - mode->height, 1,
					 mode->vts_def - mode->height);
		__v4l2_ctrl_s_ctrl(s5kjn1->vblank,
				   mode->vts_def - mode->height);
		/* Update max exposure while meeting expected vblanking */
		exposure_max = mode->vts_def - 4;
		exposure_def = (exposure_max < S5KJN1_EXPOSURE_DEFAULT) ?
			exposure_max : S5KJN1_EXPOSURE_DEFAULT;
		__v4l2_ctrl_modify_range(s5kjn1->exposure,
					 s5kjn1->exposure->minimum,
					 exposure_max, s5kjn1->exposure->step,
					 exposure_def);
		/*
		 * Currently PPL is fixed to S5KJN1_PPL_DEFAULT, so hblank
		 * depends on mode->width only, and is not changeble in any
		 * way other than changing the mode.
		 */
		hblank = S5KJN1_PPL_DEFAULT - mode->width;
		__v4l2_ctrl_modify_range(s5kjn1->hblank, hblank, hblank, 1,
					 hblank);
	}

	return 0;
}

static int s5kjn1_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_selection *sel)
{
	dev_info(sd->dev, "s5kjn1_get_selection\n");
	switch (sel->target) {
	case V4L2_SEL_TGT_CROP: {
		sel->r = *v4l2_subdev_get_pad_crop(sd, state, 0);
		return 0;
	}

	case V4L2_SEL_TGT_NATIVE_SIZE:
		sel->r.top = 0;
		sel->r.left = 0;
		sel->r.width = S5KJN1_NATIVE_WIDTH;
		sel->r.height = S5KJN1_NATIVE_HEIGHT;

		return 0;

	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_CROP_BOUNDS:
		sel->r.top = S5KJN1_PIXEL_ARRAY_TOP;
		sel->r.left = S5KJN1_PIXEL_ARRAY_LEFT;
		sel->r.width = S5KJN1_PIXEL_ARRAY_WIDTH;
		sel->r.height = S5KJN1_PIXEL_ARRAY_HEIGHT;

		return 0;
	}

	return -EINVAL;
}

static int s5kjn1_init_cfg(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *state)
{
	struct v4l2_subdev_format fmt = {
		.which = V4L2_SUBDEV_FORMAT_TRY,
		.pad = 0,
		.format = {
			.code = MEDIA_BUS_FMT_SRGGB10_1X10,
			.width = supported_modes[0].width,
			.height = supported_modes[0].height,
		},
	};

	s5kjn1_set_pad_format(sd, state, &fmt);

	return 0;
}

static const struct v4l2_subdev_core_ops s5kjn1_core_ops = {
	.subscribe_event = v4l2_ctrl_subdev_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_video_ops s5kjn1_video_ops = {
	.s_stream = s5kjn1_set_stream,
};

static const struct v4l2_subdev_pad_ops s5kjn1_pad_ops = {
	.init_cfg = s5kjn1_init_cfg,
	.enum_mbus_code = s5kjn1_enum_mbus_code,
	.get_fmt = v4l2_subdev_get_fmt,
	.set_fmt = s5kjn1_set_pad_format,
	.get_selection = s5kjn1_get_selection,
	.enum_frame_size = s5kjn1_enum_frame_size,
};

static const struct v4l2_subdev_ops s5kjn1_subdev_ops = {
	.core = &s5kjn1_core_ops,
	.video = &s5kjn1_video_ops,
	.pad = &s5kjn1_pad_ops,
};


/* -----------------------------------------------------------------------------
 * Power management
 */

static int s5kjn1_power_on(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct s5kjn1 *s5kjn1 = to_s5kjn1(sd);
	int ret;

	dev_info(dev, "s5kjn1_power_on\n");
	ret = regulator_bulk_enable(S5KJN1_NUM_SUPPLIES,
				    s5kjn1->supplies);
	if (ret) {
		dev_err(dev, "%s: failed to enable regulators\n",
			__func__);
		return ret;
	}

	ret = clk_prepare_enable(s5kjn1->xclk);
	if (ret) {
		dev_err(dev, "%s: failed to enable clock\n",
			__func__);
		goto reg_off;
	}

	gpiod_set_value_cansleep(s5kjn1->reset_gpio, 0);
	//FIXME
	usleep_range(S5KJN1_XCLR_MIN_DELAY_US,
		     S5KJN1_XCLR_MIN_DELAY_US + S5KJN1_XCLR_DELAY_RANGE_US);

	if (!IS_ERR(s5kjn1->pwdn_gpio))
		gpiod_set_value_cansleep(s5kjn1->pwdn_gpio, 1);

	return 0;

reg_off:
	regulator_bulk_disable(S5KJN1_NUM_SUPPLIES, s5kjn1->supplies);

	return ret;
}

static int s5kjn1_power_off(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct s5kjn1 *s5kjn1 = to_s5kjn1(sd);
	dev_info(dev, "s5kjn1_power_off\n");

	if (!IS_ERR(s5kjn1->pwdn_gpio))
		gpiod_set_value_cansleep(s5kjn1->pwdn_gpio, 0);

	gpiod_set_value_cansleep(s5kjn1->reset_gpio, 1);
	regulator_bulk_disable(S5KJN1_NUM_SUPPLIES, s5kjn1->supplies);
	clk_disable_unprepare(s5kjn1->xclk);

	return 0;
}

/* -----------------------------------------------------------------------------
 * Probe & remove
 */

static int s5kjn1_get_regulators(struct s5kjn1 *s5kjn1)
{
	struct i2c_client *client = v4l2_get_subdevdata(&s5kjn1->sd);
	unsigned int i;

	for (i = 0; i < S5KJN1_NUM_SUPPLIES; i++)
		s5kjn1->supplies[i].supply = s5kjn1_supply_name[i];

	return devm_regulator_bulk_get(&client->dev,
				       S5KJN1_NUM_SUPPLIES,
				       s5kjn1->supplies);
}

/* Verify chip ID */
static int s5kjn1_identify_module(struct s5kjn1 *s5kjn1)
{
	struct i2c_client *client = v4l2_get_subdevdata(&s5kjn1->sd);
	int ret;
	u64 val;

	ret = cci_read(s5kjn1->regmap, S5KJN1_REG_CHIP_ID, &val, NULL);
	if (ret) {
		dev_err(&client->dev, "failed to read chip id %x\n",
			S5KJN1_CHIP_ID);
		return ret;
	}

	if (val != S5KJN1_CHIP_ID) {
		dev_err(&client->dev, "chip id mismatch: %x!=%llx\n",
			S5KJN1_CHIP_ID, val);
		return -EIO;
	}

	return 0;
}

static int s5kjn1_check_hwcfg(struct device *dev, struct s5kjn1 *s5kjn1)
{
	struct fwnode_handle *endpoint;
	struct v4l2_fwnode_endpoint ep_cfg = {
		.bus_type = V4L2_MBUS_CSI2_DPHY
	};
	int ret = -EINVAL;

	endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(dev), NULL);
	if (!endpoint) {
		dev_err(dev, "endpoint node not found\n");
		return -EINVAL;
	}

	if (v4l2_fwnode_endpoint_alloc_parse(endpoint, &ep_cfg)) {
		dev_err(dev, "could not parse endpoint\n");
		goto error_out;
	}

	/* Check the number of MIPI CSI2 data lanes */
	if (ep_cfg.bus.mipi_csi2.num_data_lanes != 4) {
		dev_err(dev, "only 4 data lanes are currently supported\n");
		goto error_out;
	}

	/* Check the link frequency set in device tree */
	if (!ep_cfg.nr_of_link_frequencies) {
		dev_err(dev, "link-frequency property not found in DT\n");
		goto error_out;
	}

	if (ep_cfg.nr_of_link_frequencies != 1 ||
	   (ep_cfg.link_frequencies[0] != S5KJN1_DEFAULT_LINK_FREQ)) {
		dev_err(dev, "Link frequency not supported: %lld\n",
			ep_cfg.link_frequencies[0]);
		goto error_out;
	}

	ret = 0;

error_out:
	v4l2_fwnode_endpoint_free(&ep_cfg);
	fwnode_handle_put(endpoint);

	return ret;
}

static int s5kjn1_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct s5kjn1 *s5kjn1;
	int ret;

	s5kjn1 = devm_kzalloc(&client->dev, sizeof(*s5kjn1), GFP_KERNEL);
	if (!s5kjn1)
		return -ENOMEM;

	v4l2_i2c_subdev_init(&s5kjn1->sd, client, &s5kjn1_subdev_ops);

	/* Check the hardware configuration in device tree */
	if (s5kjn1_check_hwcfg(dev, s5kjn1))
		return -EINVAL;

	s5kjn1->regmap = devm_cci_regmap_init_i2c(client, 16);
	if (IS_ERR(s5kjn1->regmap)) {
		ret = PTR_ERR(s5kjn1->regmap);
		dev_err(dev, "failed to initialize CCI: %d\n", ret);
		return ret;
	}

	/* Get system clock (xclk) */
	s5kjn1->xclk = devm_clk_get(dev, NULL);
	if (IS_ERR(s5kjn1->xclk)) {
		dev_err(dev, "failed to get xclk\n");
		return PTR_ERR(s5kjn1->xclk);
	}

	s5kjn1->xclk_freq = clk_get_rate(s5kjn1->xclk);
	if (s5kjn1->xclk_freq != S5KJN1_XCLK_FREQ) {
		dev_err(dev, "xclk frequency not supported: %d Hz\n",
			s5kjn1->xclk_freq);
		return -EINVAL;
	}

	ret = s5kjn1_get_regulators(s5kjn1);
	if (ret) {
		dev_err(dev, "failed to get regulators\n");
		return ret;
	}

	/* Request optional enable pin */
	s5kjn1->reset_gpio = devm_gpiod_get_optional(dev, "reset",
						     GPIOD_OUT_LOW);

	/*
	 * The sensor must be powered for s5kjn1_identify_module()
	 * to be able to read the CHIP_ID register
	 */
	ret = s5kjn1_power_on(dev);
	if (ret)
		return ret;

	ret = s5kjn1_identify_module(s5kjn1);
	if (ret)
		goto error_power_off;

	/* Set default mode to max resolution */
	s5kjn1->mode = &supported_modes[0];

	/*
	 * Sensor doesn't enter LP-11 state upon power up until and unless
	 * streaming is started, so upon power up switch the modes to:
	 * streaming -> standby
	 */
	ret = cci_write(s5kjn1->regmap, S5KJN1_REG_MODE_SELECT,
			S5KJN1_MODE_STREAMING, NULL);
	if (ret < 0)
		goto error_power_off;

	usleep_range(100, 110);

	/* put sensor back to standby mode */
	ret = cci_write(s5kjn1->regmap, S5KJN1_REG_MODE_SELECT,
			S5KJN1_MODE_STANDBY, NULL);
	if (ret < 0)
		goto error_power_off;

	usleep_range(100, 110);

	ret = s5kjn1_init_controls(s5kjn1);
	if (ret)
		goto error_power_off;

	/* Initialize subdev */
	s5kjn1->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
			    V4L2_SUBDEV_FL_HAS_EVENTS;
	s5kjn1->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	/* Initialize source pad */
	s5kjn1->pad.flags = MEDIA_PAD_FL_SOURCE;

	ret = media_entity_pads_init(&s5kjn1->sd.entity, 1, &s5kjn1->pad);
	if (ret) {
		dev_err(dev, "failed to init entity pads: %d\n", ret);
		goto error_handler_free;
	}

	s5kjn1->sd.state_lock = s5kjn1->ctrl_handler.lock;
	ret = v4l2_subdev_init_finalize(&s5kjn1->sd);
	if (ret < 0) {
		dev_err(dev, "subdev init error: %d\n", ret);
		goto error_media_entity;
	}

	ret = v4l2_async_register_subdev_sensor(&s5kjn1->sd);
	if (ret < 0) {
		dev_err(dev, "failed to register sensor sub-device: %d\n", ret);
		goto error_subdev_cleanup;
	}

	/* Enable runtime PM and turn off the device */
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_idle(dev);

	return 0;

error_subdev_cleanup:
	v4l2_subdev_cleanup(&s5kjn1->sd);

error_media_entity:
	media_entity_cleanup(&s5kjn1->sd.entity);

error_handler_free:
	s5kjn1_free_controls(s5kjn1);

error_power_off:
	s5kjn1_power_off(dev);

	return ret;
}

static void s5kjn1_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5kjn1 *s5kjn1 = to_s5kjn1(sd);

	v4l2_async_unregister_subdev(sd);
	v4l2_subdev_cleanup(sd);
	media_entity_cleanup(&sd->entity);
	s5kjn1_free_controls(s5kjn1);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		s5kjn1_power_off(&client->dev);
	pm_runtime_set_suspended(&client->dev);
}

static const struct of_device_id s5kjn1_dt_ids[] = {
	{ .compatible = "samsung,s5kjn1" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, s5kjn1_dt_ids);

static const struct dev_pm_ops s5kjn1_pm_ops = {
	SET_RUNTIME_PM_OPS(s5kjn1_power_off, s5kjn1_power_on, NULL)
};

static struct i2c_driver s5kjn1_i2c_driver = {
	.driver = {
		.name = "s5kjn1",
		.of_match_table	= s5kjn1_dt_ids,
		.pm = &s5kjn1_pm_ops,
	},
	.probe = s5kjn1_probe,
	.remove = s5kjn1_remove,
};

module_i2c_driver(s5kjn1_i2c_driver);

MODULE_DESCRIPTION("Sony S5KJN1 sensor driver");
MODULE_LICENSE("GPL v2");
