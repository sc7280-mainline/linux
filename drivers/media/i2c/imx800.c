// SPDX-License-Identifier: GPL-2.0
/*
 * A V4L2 driver for Sony IMX800 cameras.
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
#define IMX800_REG_CHIP_ID		CCI_REG16(0x0016)
#define IMX800_CHIP_ID			0x0800

#define IMX800_REG_MODE_SELECT		CCI_REG8(0x0100)
#define IMX800_MODE_STANDBY		0x00
#define IMX800_MODE_STREAMING		0x01

/* Group hold register */
#define IMX800_REG_HOLD			CCI_REG8(0x0104)
#define IMX800_HOLD_DISABLE		0x00
#define IMX800_HOLD_ENABLE		0x01

/* Analog gain control */
#define IMX800_REG_ANALOG_GAIN		CCI_REG8(0x0204)
#define IMX800_ANA_GAIN_MIN		0x400
#define IMX800_ANA_GAIN_MAX		0x10000
#define IMX800_ANA_GAIN_DEFAULT		0x1334
#define IMX800_ANA_GAIN_STEP		1

/* Digital gain control */
#define IMX800_REG_DIGITAL_GAIN		CCI_REG16(0x020e)
#define IMX800_DGTL_GAIN_MIN		0x0100
#define IMX800_DGTL_GAIN_MAX		0x0fff
#define IMX800_DGTL_GAIN_DEFAULT	0x0100
#define IMX800_DGTL_GAIN_STEP		1

/* Exposure control */
#define IMX800_REG_EXPOSURE		CCI_REG16(0x0202)
#define IMX800_EXPOSURE_MIN		0x18
#define IMX800_EXPOSURE_MAX		0xffcc
#define IMX800_EXPOSURE_STEP		1
#define IMX800_EXPOSURE_DEFAULT		0x3d0

/* V_TIMING internal */
#define IMX800_REG_VTS			CCI_REG16(0x0160)
#define IMX800_VTS_MAX			0xffff

#define IMX800_VBLANK_MIN		4

/* HBLANK control - read only */
#define IMX800_PPL_DEFAULT		3448

#define IMX800_REG_ORIENTATION		CCI_REG8(0x0101)

/* Test Pattern Control */
#define IMX800_REG_TEST_PATTERN		CCI_REG16(0x0600)
#define IMX800_TEST_PATTERN_DISABLE	0
#define IMX800_TEST_PATTERN_SOLID_COLOR	1
#define IMX800_TEST_PATTERN_COLOR_BARS	2
#define IMX800_TEST_PATTERN_GREY_COLOR	3
#define IMX800_TEST_PATTERN_PN9		4

/* Test pattern colour components */
#define IMX800_REG_TESTP_RED		CCI_REG16(0x0602)
#define IMX800_REG_TESTP_GREENR		CCI_REG16(0x0604)
#define IMX800_REG_TESTP_BLUE		CCI_REG16(0x0606)
#define IMX800_REG_TESTP_GREENB		CCI_REG16(0x0608)
#define IMX800_TESTP_COLOUR_MIN		0
#define IMX800_TESTP_COLOUR_MAX		0x03ff
#define IMX800_TESTP_COLOUR_STEP	1

#define IMX800_REG_TP_WINDOW_WIDTH	CCI_REG16(0x0624)
#define IMX800_REG_TP_WINDOW_HEIGHT	CCI_REG16(0x0626)

/* External clock frequency is 24.0M */
#define IMX800_XCLK_FREQ		24000000

//TODO
/* Pixel rate is fixed for all the modes */
#define IMX800_PIXEL_RATE		1176690000

//TODO
#define IMX800_DEFAULT_LINK_FREQ	600000000

/* IMX800 native and active pixel array size. */
#define IMX800_NATIVE_WIDTH		4096U
#define IMX800_NATIVE_HEIGHT		3072U
#define IMX800_PIXEL_ARRAY_LEFT		0U
#define IMX800_PIXEL_ARRAY_TOP		0U
#define IMX800_PIXEL_ARRAY_WIDTH	4096U
#define IMX800_PIXEL_ARRAY_HEIGHT	3072U

struct imx800_reg_list {
	unsigned int num_of_regs;
	const struct cci_reg_sequence *regs;
};

/* Mode : resolution and related config&values */
struct imx800_mode {
	/* Frame width */
	unsigned int width;
	/* Frame height */
	unsigned int height;

	/* V-timing */
	unsigned int vts_def;

	/* Default register values */
	struct imx800_reg_list reg_list;
};

static const struct cci_reg_sequence imx800_common_regs[] = {
	//Power ON
	//Input EXTCLK
	//XCLR OFF
	//External Clock Setting
	{ CCI_REG8(0x0136), 0x18 },
	{ CCI_REG8(0x0137), 0x00 },
	{ CCI_REG8(0x3304), 0x00 },
	{ CCI_REG8(0x33f0), 0x02 },
	{ CCI_REG8(0x33f1), 0x03 },
	{ CCI_REG8(0x0111), 0x03 },
	{ CCI_REG8(0x3101), 0x0f },
	{ CCI_REG8(0x316e), 0x00 },
	{ CCI_REG8(0x3379), 0x00 },
	{ CCI_REG8(0x39d1), 0x00 },
	{ CCI_REG8(0x3a34), 0x00 },
	{ CCI_REG8(0x3a35), 0xe6 },
	{ CCI_REG8(0x3a36), 0x00 },
	{ CCI_REG8(0x3a37), 0xe6 },
	{ CCI_REG8(0x4a83), 0x66 },
	{ CCI_REG8(0x4a97), 0x52 },
	{ CCI_REG8(0x4aa5), 0x6a },
	{ CCI_REG8(0x4aab), 0x66 },
	{ CCI_REG8(0x4abf), 0x52 },
	{ CCI_REG8(0x4acd), 0x6a },
	{ CCI_REG8(0x4ad5), 0x6a },
	{ CCI_REG8(0x4b57), 0x66 },
	{ CCI_REG8(0x4b6b), 0x52 },
	{ CCI_REG8(0x4b7b), 0x6a },
	{ CCI_REG8(0x56d8), 0x2f },
	{ CCI_REG8(0x56da), 0x2f },
	{ CCI_REG8(0x56dc), 0x2f },
	{ CCI_REG8(0x56e0), 0x2f },
	{ CCI_REG8(0x56e2), 0x2f },
	{ CCI_REG8(0x56e3), 0x2f },
	{ CCI_REG8(0x56e5), 0x2f },
	{ CCI_REG8(0x56e7), 0x2f },
	{ CCI_REG8(0x56e9), 0x2f },
	{ CCI_REG8(0x56eb), 0x2f },
	{ CCI_REG8(0x56ed), 0x2f },
	{ CCI_REG8(0x56ef), 0x2f },
	{ CCI_REG8(0x5829), 0x06 },
	{ CCI_REG8(0x5844), 0x06 },
	{ CCI_REG8(0x5944), 0x00 },
	{ CCI_REG8(0x5948), 0x00 },
	{ CCI_REG8(0x594c), 0x00 },
	{ CCI_REG8(0x594e), 0x00 },
	{ CCI_REG8(0x594f), 0x00 },
	{ CCI_REG8(0x5953), 0x00 },
	{ CCI_REG8(0x5955), 0x00 },
	{ CCI_REG8(0x5957), 0x00 },
	{ CCI_REG8(0x5959), 0x00 },
	{ CCI_REG8(0x595b), 0x00 },
	{ CCI_REG8(0x5964), 0x00 },
	{ CCI_REG8(0x5966), 0x00 },
	{ CCI_REG8(0x5967), 0x00 },
	{ CCI_REG8(0x5969), 0x00 },
	{ CCI_REG8(0x596a), 0x00 },
	{ CCI_REG8(0x596b), 0x00 },
	{ CCI_REG8(0x596c), 0x00 },
	{ CCI_REG8(0x596d), 0x00 },
	{ CCI_REG8(0x5975), 0x00 },
	{ CCI_REG8(0x5979), 0x00 },
	{ CCI_REG8(0x5980), 0x00 },
	{ CCI_REG8(0x5982), 0x00 },
	{ CCI_REG8(0x5cc4), 0x20 },
	{ CCI_REG8(0x5cc8), 0x09 },
	{ CCI_REG8(0x5ccc), 0x09 },
	{ CCI_REG8(0x5cce), 0x09 },
	{ CCI_REG8(0x5ccf), 0x15 },
	{ CCI_REG8(0x5cd3), 0x04 },
	{ CCI_REG8(0x5cd5), 0x04 },
	{ CCI_REG8(0x5cd7), 0x09 },
	{ CCI_REG8(0x5cd9), 0x04 },
	{ CCI_REG8(0x5cdb), 0x04 },
	{ CCI_REG8(0x5ce4), 0x09 },
	{ CCI_REG8(0x5ce6), 0x09 },
	{ CCI_REG8(0x5ce7), 0x09 },
	{ CCI_REG8(0x5ce9), 0x04 },
	{ CCI_REG8(0x5cea), 0x04 },
	{ CCI_REG8(0x5ceb), 0x09 },
	{ CCI_REG8(0x5cec), 0x04 },
	{ CCI_REG8(0x5ced), 0x04 },
	{ CCI_REG8(0x5cf1), 0x28 },
	{ CCI_REG8(0x5cf5), 0x09 },
	{ CCI_REG8(0x5cf9), 0x09 },
	{ CCI_REG8(0x5cfb), 0x1f },
	{ CCI_REG8(0x5cfc), 0x1a },
	{ CCI_REG8(0x5d00), 0x04 },
	{ CCI_REG8(0x5d02), 0x04 },
	{ CCI_REG8(0x5d04), 0x1f },
	{ CCI_REG8(0x5d06), 0x15 },
	{ CCI_REG8(0x5d08), 0x15 },
	{ CCI_REG8(0x5d10), 0x1f },
	{ CCI_REG8(0x5d12), 0x1f },
	{ CCI_REG8(0x5d13), 0x15 },
	{ CCI_REG8(0x5d14), 0x15 },
	{ CCI_REG8(0x5d15), 0x26 },
	{ CCI_REG8(0x5d19), 0x1f },
	{ CCI_REG8(0x5d1d), 0x1f },
	{ CCI_REG8(0x5d1f), 0x2d },
	{ CCI_REG8(0x5d20), 0x1a },
	{ CCI_REG8(0x5d24), 0x15 },
	{ CCI_REG8(0x5d26), 0x15 },
	{ CCI_REG8(0x5d28), 0x2d },
	{ CCI_REG8(0x5d2a), 0x1a },
	{ CCI_REG8(0x5d2c), 0x1a },
	{ CCI_REG8(0x5d34), 0x2d },
	{ CCI_REG8(0x5d38), 0x2d },
	{ CCI_REG8(0x5d3c), 0x2d },
	{ CCI_REG8(0x5d3e), 0x2d },
	{ CCI_REG8(0x5d3f), 0x1e },
	{ CCI_REG8(0x5d43), 0x1a },
	{ CCI_REG8(0x5d45), 0x1a },
	{ CCI_REG8(0x5d47), 0x2d },
	{ CCI_REG8(0x5d49), 0x1a },
	{ CCI_REG8(0x5d4b), 0x1a },
	{ CCI_REG8(0x5d54), 0x2d },
	{ CCI_REG8(0x5d56), 0x2d },
	{ CCI_REG8(0x5d57), 0x2d },
	{ CCI_REG8(0x5d59), 0x1a },
	{ CCI_REG8(0x5d5a), 0x1a },
	{ CCI_REG8(0x5d5b), 0x2d },
	{ CCI_REG8(0x5d5c), 0x1e },
	{ CCI_REG8(0x5d5d), 0x1e },
	{ CCI_REG8(0x5d61), 0x2d },
	{ CCI_REG8(0x5d63), 0x2d },
	{ CCI_REG8(0x5d64), 0x1e },
	{ CCI_REG8(0x5d65), 0x1e },
	{ CCI_REG8(0x5d66), 0x28 },
	{ CCI_REG8(0x5d6a), 0x0f },
	{ CCI_REG8(0x5d6e), 0x0f },
	{ CCI_REG8(0x5d70), 0x0f },
	{ CCI_REG8(0x5d71), 0x6c },
	{ CCI_REG8(0x5d75), 0x2d },
	{ CCI_REG8(0x5d77), 0x2d },
	{ CCI_REG8(0x5d79), 0x0f },
	{ CCI_REG8(0x5d7b), 0x2d },
	{ CCI_REG8(0x5d7d), 0x2d },
	{ CCI_REG8(0x5d86), 0x0f },
	{ CCI_REG8(0x5d88), 0x0f },
	{ CCI_REG8(0x5d89), 0x0f },
	{ CCI_REG8(0x5d8b), 0x2d },
	{ CCI_REG8(0x5d8c), 0x2d },
	{ CCI_REG8(0x5d8d), 0x0f },
	{ CCI_REG8(0x5d8e), 0x2d },
	{ CCI_REG8(0x5d8f), 0x2d },
	{ CCI_REG8(0x5d93), 0x79 },
	{ CCI_REG8(0x5d97), 0x0f },
	{ CCI_REG8(0x5d9b), 0x0f },
	{ CCI_REG8(0x5d9d), 0x2d },
	{ CCI_REG8(0x5d9e), 0x7b },
	{ CCI_REG8(0x5da2), 0x2d },
	{ CCI_REG8(0x5da4), 0x2d },
	{ CCI_REG8(0x5da6), 0x2d },
	{ CCI_REG8(0x5da8), 0x6c },
	{ CCI_REG8(0x5daa), 0x6c },
	{ CCI_REG8(0x5db2), 0x2d },
	{ CCI_REG8(0x5db4), 0x2d },
	{ CCI_REG8(0x5db5), 0x6c },
	{ CCI_REG8(0x5db6), 0x6c },
	{ CCI_REG8(0x5db7), 0x5a },
	{ CCI_REG8(0x5dbb), 0x2d },
	{ CCI_REG8(0x5dbf), 0x2d },
	{ CCI_REG8(0x5dc1), 0x32 },
	{ CCI_REG8(0x5dc2), 0x7d },
	{ CCI_REG8(0x5dc6), 0x6c },
	{ CCI_REG8(0x5dc8), 0x6c },
	{ CCI_REG8(0x5dca), 0x32 },
	{ CCI_REG8(0x5dcc), 0x7b },
	{ CCI_REG8(0x5dce), 0x7b },
	{ CCI_REG8(0x5dd6), 0x2d },
	{ CCI_REG8(0x5dda), 0x32 },
	{ CCI_REG8(0x5dde), 0x32 },
	{ CCI_REG8(0x5de0), 0x1e },
	{ CCI_REG8(0x5de1), 0x7a },
	{ CCI_REG8(0x5de5), 0x7b },
	{ CCI_REG8(0x5de7), 0x7b },
	{ CCI_REG8(0x5de9), 0x1e },
	{ CCI_REG8(0x5deb), 0x7d },
	{ CCI_REG8(0x5ded), 0x7d },
	{ CCI_REG8(0x5df6), 0x1e },
	{ CCI_REG8(0x5df8), 0x1e },
	{ CCI_REG8(0x5df9), 0x28 },
	{ CCI_REG8(0x5dfb), 0x7d },
	{ CCI_REG8(0x5dfc), 0x7d },
	{ CCI_REG8(0x5dfd), 0x28 },
	{ CCI_REG8(0x5dfe), 0x7a },
	{ CCI_REG8(0x5dff), 0x7a },
	{ CCI_REG8(0x5e03), 0x28 },
	{ CCI_REG8(0x5e05), 0x28 },
	{ CCI_REG8(0x5e06), 0x7a },
	{ CCI_REG8(0x5e07), 0x7a },
	{ CCI_REG8(0x6132), 0x2d },
	{ CCI_REG8(0x6136), 0x0f },
	{ CCI_REG8(0x613a), 0x0f },
	{ CCI_REG8(0x613c), 0x23 },
	{ CCI_REG8(0x613d), 0x2d },
	{ CCI_REG8(0x6141), 0x0f },
	{ CCI_REG8(0x6143), 0x0f },
	{ CCI_REG8(0x6145), 0x23 },
	{ CCI_REG8(0x6147), 0x23 },
	{ CCI_REG8(0x6149), 0x23 },
	{ CCI_REG8(0x6151), 0x2d },
	{ CCI_REG8(0x6155), 0x0f },
	{ CCI_REG8(0x6159), 0x0f },
	{ CCI_REG8(0x615b), 0x23 },
	{ CCI_REG8(0x615c), 0x2d },
	{ CCI_REG8(0x6160), 0x0f },
	{ CCI_REG8(0x6162), 0x0f },
	{ CCI_REG8(0x6164), 0x23 },
	{ CCI_REG8(0x6166), 0x23 },
	{ CCI_REG8(0x6168), 0x23 },
	{ CCI_REG8(0x6174), 0x0f },
	{ CCI_REG8(0x6178), 0x0f },
	{ CCI_REG8(0x617a), 0x2d },
	{ CCI_REG8(0x617f), 0x0f },
	{ CCI_REG8(0x6181), 0x0f },
	{ CCI_REG8(0x6183), 0x2d },
	{ CCI_REG8(0x6185), 0x2d },
	{ CCI_REG8(0x6187), 0x2d },
	{ CCI_REG8(0x6193), 0x23 },
	{ CCI_REG8(0x6197), 0x23 },
	{ CCI_REG8(0x6199), 0x2d },
	{ CCI_REG8(0x619e), 0x23 },
	{ CCI_REG8(0x61a0), 0x23 },
	{ CCI_REG8(0x61a2), 0x2d },
	{ CCI_REG8(0x61a4), 0x2d },
	{ CCI_REG8(0x61a6), 0x2d },
	{ CCI_REG8(0x61b2), 0x23 },
	{ CCI_REG8(0x61b6), 0x23 },
	{ CCI_REG8(0x61bd), 0x23 },
	{ CCI_REG8(0x61bf), 0x23 },
	{ CCI_REG8(0x6201), 0x28 },
	{ CCI_REG8(0x6203), 0x28 },
	{ CCI_REG8(0x6207), 0x28 },
	{ CCI_REG8(0x6209), 0x28 },
	{ CCI_REG8(0x620c), 0x28 },
	{ CCI_REG8(0x620e), 0x28 },
	{ CCI_REG8(0x6210), 0x28 },
	{ CCI_REG8(0x6212), 0x28 },
	{ CCI_REG8(0x6214), 0x28 },
	{ CCI_REG8(0x6216), 0x28 },
	{ CCI_REG8(0x6220), 0x28 },
	{ CCI_REG8(0x6222), 0x28 },
	{ CCI_REG8(0x6224), 0x28 },
	{ CCI_REG8(0x6226), 0x28 },
	{ CCI_REG8(0x6229), 0x28 },
	{ CCI_REG8(0x622b), 0x28 },
	{ CCI_REG8(0x622f), 0x28 },
	{ CCI_REG8(0x6231), 0x28 },
	{ CCI_REG8(0x6234), 0x28 },
	{ CCI_REG8(0x6236), 0x28 },
	{ CCI_REG8(0x6238), 0x28 },
	{ CCI_REG8(0x623a), 0x28 },
	{ CCI_REG8(0x623c), 0x28 },
	{ CCI_REG8(0x623e), 0x28 },
	{ CCI_REG8(0x636b), 0x02 },
	{ CCI_REG8(0x636f), 0x02 },
	{ CCI_REG8(0x6376), 0x02 },
	{ CCI_REG8(0x6378), 0x02 },
	{ CCI_REG8(0x638a), 0x11 },
	{ CCI_REG8(0x638e), 0x11 },
	{ CCI_REG8(0x63a6), 0x11 },
	{ CCI_REG8(0x63a8), 0x11 },
	{ CCI_REG8(0x63b7), 0x11 },
	{ CCI_REG8(0x63bb), 0x11 },
	{ CCI_REG8(0x63d2), 0x11 },
	{ CCI_REG8(0x63d4), 0x11 },
	{ CCI_REG8(0x63d5), 0x13 },
	{ CCI_REG8(0x63d6), 0x13 },
	{ CCI_REG8(0x63db), 0x11 },
	{ CCI_REG8(0x63df), 0x11 },
	{ CCI_REG8(0x63e6), 0x13 },
	{ CCI_REG8(0x63e8), 0x13 },
	{ CCI_REG8(0x63fa), 0x11 },
	{ CCI_REG8(0x63fe), 0x11 },
	{ CCI_REG8(0x6429), 0x20 },
	{ CCI_REG8(0x642d), 0x20 },
	{ CCI_REG8(0x6431), 0x19 },
	{ CCI_REG8(0x6439), 0x19 },
	{ CCI_REG8(0x643d), 0x20 },
	{ CCI_REG8(0x643f), 0x20 },
	{ CCI_REG8(0x6443), 0x20 },
	{ CCI_REG8(0x6447), 0x19 },
	{ CCI_REG8(0x644b), 0x19 },
	{ CCI_REG8(0x644f), 0x20 },
	{ CCI_REG8(0x6453), 0x20 },
	{ CCI_REG8(0x6457), 0x20 },
	{ CCI_REG8(0x6467), 0x20 },
	{ CCI_REG8(0x6469), 0x19 },
	{ CCI_REG8(0x646d), 0x19 },
	{ CCI_REG8(0x646f), 0x20 },
	{ CCI_REG8(0x6471), 0x20 },
	{ CCI_REG8(0x6473), 0x19 },
	{ CCI_REG8(0x6475), 0x19 },
	{ CCI_REG8(0x6477), 0x20 },
	{ CCI_REG8(0x6479), 0x20 },
	{ CCI_REG8(0x647b), 0x20 },
	{ CCI_REG8(0x6487), 0x20 },
	{ CCI_REG8(0x648b), 0x20 },
	{ CCI_REG8(0x6493), 0x20 },
	{ CCI_REG8(0x6497), 0x20 },
	{ CCI_REG8(0x649d), 0x20 },
	{ CCI_REG8(0x64a1), 0x20 },
	{ CCI_REG8(0x64a5), 0x20 },
	{ CCI_REG8(0x64a9), 0x20 },
	{ CCI_REG8(0x64ad), 0x20 },
	{ CCI_REG8(0x64b1), 0x20 },
	{ CCI_REG8(0x64c1), 0x21 },
	{ CCI_REG8(0x64c5), 0x21 },
	{ CCI_REG8(0x64c7), 0x21 },
	{ CCI_REG8(0x64c9), 0x21 },
	{ CCI_REG8(0x64d3), 0x21 },
	{ CCI_REG8(0x64db), 0x21 },
	{ CCI_REG8(0x64e9), 0x21 },
	{ CCI_REG8(0x64ed), 0x21 },
	{ CCI_REG8(0x656d), 0x61 },
	{ CCI_REG8(0x6571), 0x61 },
	{ CCI_REG8(0x6575), 0x5a },
	{ CCI_REG8(0x657d), 0x5a },
	{ CCI_REG8(0x6581), 0x61 },
	{ CCI_REG8(0x6583), 0x61 },
	{ CCI_REG8(0x6587), 0x61 },
	{ CCI_REG8(0x658b), 0x5a },
	{ CCI_REG8(0x658f), 0x5a },
	{ CCI_REG8(0x6593), 0x61 },
	{ CCI_REG8(0x6597), 0x61 },
	{ CCI_REG8(0x659b), 0x61 },
	{ CCI_REG8(0x65ab), 0x61 },
	{ CCI_REG8(0x65ad), 0x5a },
	{ CCI_REG8(0x65b1), 0x5a },
	{ CCI_REG8(0x65b3), 0x61 },
	{ CCI_REG8(0x65b5), 0x61 },
	{ CCI_REG8(0x65b7), 0x5a },
	{ CCI_REG8(0x65b9), 0x5a },
	{ CCI_REG8(0x65bb), 0x61 },
	{ CCI_REG8(0x65bd), 0x61 },
	{ CCI_REG8(0x65bf), 0x61 },
	{ CCI_REG8(0x65cb), 0x61 },
	{ CCI_REG8(0x65cf), 0x61 },
	{ CCI_REG8(0x65d7), 0x61 },
	{ CCI_REG8(0x65db), 0x61 },
	{ CCI_REG8(0x65e1), 0x61 },
	{ CCI_REG8(0x65e5), 0x61 },
	{ CCI_REG8(0x65e9), 0x61 },
	{ CCI_REG8(0x65ed), 0x61 },
	{ CCI_REG8(0x65f1), 0x61 },
	{ CCI_REG8(0x65f5), 0x61 },
	{ CCI_REG8(0x6605), 0x62 },
	{ CCI_REG8(0x6609), 0x62 },
	{ CCI_REG8(0x660b), 0x62 },
	{ CCI_REG8(0x660d), 0x62 },
	{ CCI_REG8(0x6617), 0x62 },
	{ CCI_REG8(0x661f), 0x62 },
	{ CCI_REG8(0x662d), 0x62 },
	{ CCI_REG8(0x6631), 0x62 },
	{ CCI_REG8(0x66b1), 0x61 },
	{ CCI_REG8(0x66b5), 0x5a },
	{ CCI_REG8(0x66b9), 0x61 },
	{ CCI_REG8(0x66c1), 0x5a },
	{ CCI_REG8(0x66c5), 0x61 },
	{ CCI_REG8(0x66c7), 0x5a },
	{ CCI_REG8(0x66c9), 0x61 },
	{ CCI_REG8(0x66cb), 0x5a },
	{ CCI_REG8(0x66cd), 0x61 },
	{ CCI_REG8(0x66d3), 0x61 },
	{ CCI_REG8(0x66d7), 0x61 },
	{ CCI_REG8(0x66df), 0x61 },
	{ CCI_REG8(0x66e3), 0x61 },
	{ CCI_REG8(0x66e5), 0x62 },
	{ CCI_REG8(0x66e7), 0x62 },
	{ CCI_REG8(0x66ed), 0x62 },
	{ CCI_REG8(0x66f9), 0x62 },
	{ CCI_REG8(0x6720), 0x1e },
	{ CCI_REG8(0x6724), 0x1e },
	{ CCI_REG8(0x6728), 0x1e },
	{ CCI_REG8(0x672a), 0x1e },
	{ CCI_REG8(0x672b), 0x1e },
	{ CCI_REG8(0x672f), 0x1e },
	{ CCI_REG8(0x6731), 0x1e },
	{ CCI_REG8(0x6733), 0x1e },
	{ CCI_REG8(0x6735), 0x1e },
	{ CCI_REG8(0x6737), 0x1e },
	{ CCI_REG8(0x6740), 0x1e },
	{ CCI_REG8(0x6742), 0x1e },
	{ CCI_REG8(0x6743), 0x1e },
	{ CCI_REG8(0x6745), 0x1e },
	{ CCI_REG8(0x6746), 0x1e },
	{ CCI_REG8(0x6747), 0x1e },
	{ CCI_REG8(0x6748), 0x1e },
	{ CCI_REG8(0x6749), 0x1e },
	{ CCI_REG8(0x674d), 0x1e },
	{ CCI_REG8(0x6751), 0x1e },
	{ CCI_REG8(0x6755), 0x1e },
	{ CCI_REG8(0x6757), 0x1e },
	{ CCI_REG8(0x6758), 0x1e },
	{ CCI_REG8(0x675c), 0x1e },
	{ CCI_REG8(0x675e), 0x1e },
	{ CCI_REG8(0x6760), 0x1e },
	{ CCI_REG8(0x6762), 0x1e },
	{ CCI_REG8(0x6764), 0x1e },
	{ CCI_REG8(0x676c), 0x1e },
	{ CCI_REG8(0x676e), 0x1e },
	{ CCI_REG8(0x676f), 0x1e },
	{ CCI_REG8(0x6770), 0x1e },
	{ CCI_REG8(0x6771), 0x1e },
	{ CCI_REG8(0x6775), 0x1e },
	{ CCI_REG8(0x6779), 0x1e },
	{ CCI_REG8(0x677b), 0x1e },
	{ CCI_REG8(0x677c), 0x1e },
	{ CCI_REG8(0x6780), 0x1e },
	{ CCI_REG8(0x6782), 0x1e },
	{ CCI_REG8(0x6784), 0x1e },
	{ CCI_REG8(0x6786), 0x1e },
	{ CCI_REG8(0x6788), 0x1e },
	{ CCI_REG8(0x6790), 0x1e },
	{ CCI_REG8(0x6794), 0x1e },
	{ CCI_REG8(0x6798), 0x1e },
	{ CCI_REG8(0x679a), 0x1e },
	{ CCI_REG8(0x679b), 0x1e },
	{ CCI_REG8(0x679f), 0x1e },
	{ CCI_REG8(0x67a1), 0x1e },
	{ CCI_REG8(0x67a3), 0x1e },
	{ CCI_REG8(0x67a5), 0x1e },
	{ CCI_REG8(0x67a7), 0x1e },
	{ CCI_REG8(0x67b0), 0x1e },
	{ CCI_REG8(0x67b2), 0x1e },
	{ CCI_REG8(0x67b3), 0x1e },
	{ CCI_REG8(0x67b5), 0x1e },
	{ CCI_REG8(0x67b6), 0x1e },
	{ CCI_REG8(0x67b7), 0x1e },
	{ CCI_REG8(0x67b8), 0x1e },
	{ CCI_REG8(0x67bc), 0x1e },
	{ CCI_REG8(0x67be), 0x1e },
	{ CCI_REG8(0x67bf), 0x1e },
	{ CCI_REG8(0x67c0), 0x1e },
	{ CCI_REG8(0x7514), 0x08 },
	{ CCI_REG8(0x7732), 0x03 },
	{ CCI_REG8(0x7bb3), 0x40 },
	{ CCI_REG8(0x7bb7), 0xbf },
	{ CCI_REG8(0x7bba), 0x01 },
	{ CCI_REG8(0x7bbb), 0x03 },
	{ CCI_REG8(0x7bbc), 0x81 },
	{ CCI_REG8(0x7bbd), 0x08 },
	{ CCI_REG8(0x7bbe), 0x81 },
	{ CCI_REG8(0x7bbf), 0x18 },
	{ CCI_REG8(0x7bc0), 0x48 },
	{ CCI_REG8(0x7bc1), 0x48 },
	{ CCI_REG8(0x7bc2), 0x48 },
	{ CCI_REG8(0x7bc3), 0x58 },
	{ CCI_REG8(0x7bc4), 0x46 },
	{ CCI_REG8(0x7bc5), 0x80 },
	{ CCI_REG8(0x7bc6), 0x46 },
	{ CCI_REG8(0x7bc7), 0x90 },
	{ CCI_REG8(0x7bc8), 0x8f },
	{ CCI_REG8(0x7bc9), 0xc0 },
	{ CCI_REG8(0x7bca), 0x8f },
	{ CCI_REG8(0x7bcb), 0xd0 },
	{ CCI_REG8(0x7d67), 0x40 },
	{ CCI_REG8(0x7d6b), 0xbf },
	{ CCI_REG8(0x7d6e), 0x01 },
	{ CCI_REG8(0x7d6f), 0x03 },
	{ CCI_REG8(0x7d90), 0x81 },
	{ CCI_REG8(0x7d91), 0x08 },
	{ CCI_REG8(0x7d92), 0x81 },
	{ CCI_REG8(0x7d93), 0x18 },
	{ CCI_REG8(0x7d94), 0x48 },
	{ CCI_REG8(0x7d95), 0x48 },
	{ CCI_REG8(0x7d96), 0x48 },
	{ CCI_REG8(0x7d97), 0x58 },
	{ CCI_REG8(0x7d98), 0x46 },
	{ CCI_REG8(0x7d99), 0x80 },
	{ CCI_REG8(0x7d9a), 0x46 },
	{ CCI_REG8(0x7d9b), 0x90 },
	{ CCI_REG8(0x7d9c), 0x8f },
	{ CCI_REG8(0x7d9d), 0xc0 },
	{ CCI_REG8(0x7d9e), 0x8f },
	{ CCI_REG8(0x7d9f), 0xd0 },
	{ CCI_REG8(0x86a9), 0x4e },
	{ CCI_REG8(0x9002), 0x08 },
	{ CCI_REG8(0x9003), 0x08 },
	{ CCI_REG8(0x9004), 0x10 },
	{ CCI_REG8(0x90b7), 0xb0 },
	{ CCI_REG8(0x90b9), 0xb0 },
	{ CCI_REG8(0x90d7), 0xb0 },
	{ CCI_REG8(0x90e4), 0x08 },
	{ CCI_REG8(0x90e5), 0x08 },
	{ CCI_REG8(0x90e6), 0x10 },
	{ CCI_REG8(0x9230), 0xbd },
	{ CCI_REG8(0x9231), 0x07 },
	{ CCI_REG8(0x9232), 0xbd },
	{ CCI_REG8(0x9233), 0x01 },
	{ CCI_REG8(0x9234), 0xbd },
	{ CCI_REG8(0x9235), 0x02 },
	{ CCI_REG8(0x9236), 0x86 },
	{ CCI_REG8(0x9237), 0xda },
	{ CCI_REG8(0x9238), 0xb5 },
	{ CCI_REG8(0x9239), 0x22 },
	{ CCI_REG8(0x923a), 0xb6 },
	{ CCI_REG8(0x923b), 0x6d },
	{ CCI_REG8(0x923c), 0x31 },
	{ CCI_REG8(0x923d), 0xbc },
	{ CCI_REG8(0x923e), 0xb5 },
	{ CCI_REG8(0x923f), 0x23 },
	{ CCI_REG8(0xb507), 0x40 },
	{ CCI_REG8(0xb50b), 0xbf },
	{ CCI_REG8(0xb50e), 0x03 },
	{ CCI_REG8(0xbcaf), 0x01 },
	{ CCI_REG8(0xbd4e), 0x0f },
	{ CCI_REG8(0xbd4f), 0x20 },
	{ CCI_REG8(0xbd56), 0x0f },
	{ CCI_REG8(0xbd57), 0x20 },
	{ CCI_REG8(0xbd5a), 0x0f },
	{ CCI_REG8(0xbd5b), 0x20 },
	{ CCI_REG8(0xbd62), 0x11 },
	{ CCI_REG8(0xbd63), 0xb0 },
	{ CCI_REG8(0xbd66), 0x2d },
	{ CCI_REG8(0xbd6a), 0x11 },
	{ CCI_REG8(0xbd6b), 0xb0 },
	{ CCI_REG8(0xbd6e), 0x2d },
	{ CCI_REG8(0xbd73), 0x00 },
	{ CCI_REG8(0xbd7b), 0x00 },
	{ CCI_REG8(0xbd7f), 0x00 },
	{ CCI_REG8(0xbd82), 0x1b },
	{ CCI_REG8(0xbd86), 0x36 },
	{ CCI_REG8(0xbd8a), 0x1b },
	{ CCI_REG8(0xbd8e), 0x36 },
	{ CCI_REG8(0xbd92), 0x0f },
	{ CCI_REG8(0xbd93), 0x20 },
	{ CCI_REG8(0xbd97), 0x00 },
	{ CCI_REG8(0xbd9a), 0x1b },
	{ CCI_REG8(0xbd9e), 0x36 },
	{ CCI_REG8(0xbdb6), 0x11 },
	{ CCI_REG8(0xbdb7), 0xb0 },
	{ CCI_REG8(0xbdba), 0x2d },
	{ CCI_REG8(0x3209), 0x01 },
	{ CCI_REG8(0x3533), 0x10 },
	{ CCI_REG8(0xa248), 0x10 },
	{ CCI_REG8(0xa249), 0x10 },
	{ CCI_REG8(0xa24a), 0x10 },
	{ CCI_REG8(0xa71b), 0x0d },
	{ CCI_REG8(0xa71d), 0x10 },
	{ CCI_REG8(0xa721), 0x0d },
	{ CCI_REG8(0xa723), 0x10 },
	{ CCI_REG8(0xa727), 0x0d },
	{ CCI_REG8(0xa729), 0x10 },
	{ CCI_REG8(0xa73f), 0x9d },
	{ CCI_REG8(0xa741), 0xc4 },
	{ CCI_REG8(0xa745), 0x9d },
	{ CCI_REG8(0xa747), 0xc4 },
	{ CCI_REG8(0xa74b), 0x9d },
	{ CCI_REG8(0xa74d), 0xc4 },
	{ CCI_REG8(0xa751), 0x0a },
	{ CCI_REG8(0xa753), 0x0b },
	{ CCI_REG8(0xa757), 0x0a },
	{ CCI_REG8(0xa759), 0x0b },
	{ CCI_REG8(0xa75d), 0x0a },
	{ CCI_REG8(0xa75f), 0x0b },
	{ CCI_REG8(0xa761), 0xb4 },
	{ CCI_REG8(0xa763), 0x56 },
	{ CCI_REG8(0xa767), 0xb4 },
	{ CCI_REG8(0xa769), 0x56 },
	{ CCI_REG8(0xa76d), 0xb4 },
	{ CCI_REG8(0xa76f), 0x56 },
	{ CCI_REG8(0xc06d), 0x0a },
	{ CCI_REG8(0xc06e), 0x0a },
	{ CCI_REG8(0xc070), 0x0a },
	{ CCI_REG8(0xc071), 0x0a },
	{ CCI_REG8(0xc073), 0x0a },
	{ CCI_REG8(0xc074), 0x0a },
	{ CCI_REG8(0xc076), 0x0a },
	{ CCI_REG8(0xc077), 0x0a },
	{ CCI_REG8(0xc086), 0x02 },
	{ CCI_REG8(0xc089), 0x02 },
	{ CCI_REG8(0xc08c), 0x02 },
	{ CCI_REG8(0xc08f), 0x02 },
	{ CCI_REG8(0xc0b5), 0x0a },
	{ CCI_REG8(0xc0b6), 0x0a },
	{ CCI_REG8(0xc0b8), 0x0a },
	{ CCI_REG8(0xc0b9), 0x0a },
	{ CCI_REG8(0xc0bb), 0x0a },
	{ CCI_REG8(0xc0bc), 0x0a },
	{ CCI_REG8(0xc0be), 0x0a },
	{ CCI_REG8(0xc0bf), 0x0a },
	{ CCI_REG8(0xc0ce), 0x02 },
	{ CCI_REG8(0xc0d1), 0x02 },
	{ CCI_REG8(0xc0d4), 0x02 },
	{ CCI_REG8(0xc0d7), 0x02 },
	{ CCI_REG8(0xcb89), 0x2d },
	{ CCI_REG8(0xcb8f), 0x2d },
	{ CCI_REG8(0xcbab), 0x23 },
	{ CCI_REG8(0xcbad), 0x23 },
	{ CCI_REG8(0xcbb1), 0x23 },
	{ CCI_REG8(0xcbb3), 0x23 },
	{ CCI_REG8(0xcc71), 0x2d },
	{ CCI_REG8(0xcc77), 0x2d },
	{ CCI_REG8(0xcc93), 0x23 },
	{ CCI_REG8(0xcc95), 0x23 },
	{ CCI_REG8(0xcc99), 0x23 },
	{ CCI_REG8(0xcc9b), 0x23 },
	{ CCI_REG8(0xcd71), 0x51 },
	{ CCI_REG8(0xcd77), 0x51 },
	{ CCI_REG8(0xcd7d), 0x51 },
	{ CCI_REG8(0xcd83), 0x51 },
	{ CCI_REG8(0xf006), 0x03 },
	{ CCI_REG8(0xf007), 0x20 },
	{ CCI_REG8(0xf008), 0x03 },
	{ CCI_REG8(0xf009), 0x20 },
	{ CCI_REG8(0xf00a), 0x03 },
	{ CCI_REG8(0xf00b), 0x20 },
	{ CCI_REG8(0xf012), 0x03 },
	{ CCI_REG8(0xf013), 0x84 },
	{ CCI_REG8(0xf014), 0x03 },
	{ CCI_REG8(0xf015), 0x84 },
	{ CCI_REG8(0xf016), 0x03 },
	{ CCI_REG8(0xf017), 0x84 },
	{ CCI_REG8(0xf072), 0x03 },
	{ CCI_REG8(0xf073), 0x20 },
	{ CCI_REG8(0xf074), 0x03 },
	{ CCI_REG8(0xf075), 0x20 },
	{ CCI_REG8(0xf076), 0x03 },
	{ CCI_REG8(0xf077), 0x20 },
	{ CCI_REG8(0xf07f), 0x84 },
	{ CCI_REG8(0xf081), 0x84 },
	{ CCI_REG8(0xf083), 0x84 },
	{ CCI_REG8(0xf501), 0x01 },
};

/*

changing values:
// integration / gain
0x202 // CCS_R_COARSE_INTEGRATION_TIME

// clock
0x302 // CCS_R_VT_SYS_CLK_DIV
0x306 // CCS_R_PLL_MULTIPLIER
0x30a // CCS_R_OP_SYS_CLK_DIV
0x30c // CCS_R_OP_PRE_PLL_CLK_DIV
0x30e // CCS_R_OP_PLL_MULTIPLIER

// line length
0x340 // CCS_R_FRAME_LENGTH_LINES
0x342 // CCS_R_LINE_LENGTH_PCK
// ROI setting
0x346 // CCS_R_Y_ADDR_START
0x34a // CCS_R_Y_ADDR_END
0x34c // CCS_R_X_OUTPUT_SIZE
0x34e // CCS_R_Y_OUTPUT_SIZE

0x40c
0x40e

only in pre
0x3086, 0x02,//phase_pix_1_vcid
0x3087, 0x2b,//phase_pix_1_DT
//global timing
0x0808, 0x02,
0x084E, 0x00,
0x084F, 0x07,
0x0850, 0x00,
0x0851, 0x07,
0x0852, 0x00,
0x0853, 0x13,
//same
0x0854, 0x00,
0x0855, 0x29,
0x0858, 0x00,
0x0859, 0x1F,
*/

static const struct cci_reg_sequence mode_4096x3072_regs[] = {
	//QBIN_Vbin_30FPS
	//H: 4096
	//V: 3072
	//MIPI output setting
	{ CCI_REG8(0x0202), 0x0C },
	{ CCI_REG8(0x0203), 0x66 },
	{ CCI_REG8(0x0301), 0x08 },
	{ CCI_REG8(0x0303), 0x04 },
	{ CCI_REG8(0x0305), 0x04 },
	{ CCI_REG8(0x0306), 0x01 },
	{ CCI_REG8(0x0307), 0x3B },
	{ CCI_REG8(0x030B), 0x04 },
	{ CCI_REG8(0x030D), 0x02 },
	{ CCI_REG8(0x030E), 0x01 },
	{ CCI_REG8(0x030F), 0x1B },
	{ CCI_REG8(0x0340), 0x0C },
	{ CCI_REG8(0x0341), 0x96 },
	{ CCI_REG8(0x0342), 0x26 },
	{ CCI_REG8(0x0343), 0x30 },
	{ CCI_REG8(0x0344), 0x01 },
	{ CCI_REG8(0x0345), 0x20 },
	{ CCI_REG8(0x0346), 0x00 },
	{ CCI_REG8(0x0347), 0x00 },
	{ CCI_REG8(0x0348), 0x21 },
	{ CCI_REG8(0x0349), 0x1F },
	{ CCI_REG8(0x034A), 0x17 },
	{ CCI_REG8(0x034B), 0xFF },
	{ CCI_REG8(0x034C), 0x10 },
	{ CCI_REG8(0x034D), 0x00 },
	{ CCI_REG8(0x034E), 0x0C },
	{ CCI_REG8(0x034F), 0x00 },
	{ CCI_REG8(0x0408), 0x00 },
	{ CCI_REG8(0x0409), 0x00 },
	{ CCI_REG8(0x040A), 0x00 },
	{ CCI_REG8(0x040B), 0x00 },
	{ CCI_REG8(0x040C), 0x10 },
	{ CCI_REG8(0x040D), 0x00 },
	{ CCI_REG8(0x040E), 0x0C },
	{ CCI_REG8(0x040F), 0x00 },
	{ CCI_REG8(0x3086), 0x02 },//phase_pix_1_vcid
	{ CCI_REG8(0x3087), 0x2b },//phase_pix_1_DT
	//global timing
	{ CCI_REG8(0x0808), 0x02 },
	{ CCI_REG8(0x084E), 0x00 },
	{ CCI_REG8(0x084F), 0x07 },
	{ CCI_REG8(0x0850), 0x00 },
	{ CCI_REG8(0x0851), 0x07 },
	{ CCI_REG8(0x0852), 0x00 },
	{ CCI_REG8(0x0853), 0x13 },
	//same
	{ CCI_REG8(0x0854), 0x00 },
	{ CCI_REG8(0x0855), 0x29 },
	{ CCI_REG8(0x0858), 0x00 },
	{ CCI_REG8(0x0859), 0x1F },
};

static const s64 imx800_link_freq_menu[] = {
	IMX800_DEFAULT_LINK_FREQ,
};

static const char * const imx800_test_pattern_menu[] = {
	"Disabled",
	"Color Bars",
	"Solid Color",
	"Grey Color Bars",
	"PN9"
};

static const int imx800_test_pattern_val[] = {
	IMX800_TEST_PATTERN_DISABLE,
	IMX800_TEST_PATTERN_COLOR_BARS,
	IMX800_TEST_PATTERN_SOLID_COLOR,
	IMX800_TEST_PATTERN_GREY_COLOR,
	IMX800_TEST_PATTERN_PN9,
};

/* regulator supplies */
static const char * const imx800_supply_name[] = {
	/* Supplies can be enabled in any order */
	"vana1",  /* Analog (2.8V) supply */
	"vana2",  /* Analog (1.8V) supply */
	"vdig",  /* Digital Core (1.1V) supply */
	"vif",  /* IF (1.2V or 1.8 V) supply */
};

#define IMX800_NUM_SUPPLIES ARRAY_SIZE(imx800_supply_name)

/*
 * The supported formats.
 * This table MUST contain 4 entries per format, to cover the various flip
 * combinations in the order
 * - no flip
 * - h flip
 * - v flip
 * - h&v flips
 */
static const u32 imx800_mbus_formats[] = {
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
#define IMX800_XCLR_MIN_DELAY_US	6200
#define IMX800_XCLR_DELAY_RANGE_US	1000

/* Mode configs */
static const struct imx800_mode supported_modes[] = {
	{
		.width = 4096,
		.height = 3072,
		.vts_def = 3526,
		.reg_list = {
			.num_of_regs = ARRAY_SIZE(mode_4096x3072_regs),
			.regs = mode_4096x3072_regs,
		},
	},
};

struct imx800 {
	struct v4l2_subdev sd;
	struct media_pad pad;

	struct regmap *regmap;
	struct clk *xclk; /* system clock to IMX800 */
	u32 xclk_freq;

	struct gpio_desc *reset_gpio;
	struct regulator_bulk_data supplies[IMX800_NUM_SUPPLIES];

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
	const struct imx800_mode *mode;
};

static inline struct imx800 *to_imx800(struct v4l2_subdev *_sd)
{
	return container_of(_sd, struct imx800, sd);
}

/* Get bayer order based on flip setting. */
static u32 imx800_get_format_code(struct imx800 *imx800, u32 code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(imx800_mbus_formats); i++)
		if (imx800_mbus_formats[i] == code)
			break;

	if (i >= ARRAY_SIZE(imx800_mbus_formats))
		i = 0;

	i = (i & ~3) | (imx800->vflip->val ? 2 : 0) |
	    (imx800->hflip->val ? 1 : 0);

	return imx800_mbus_formats[i];
}

/* -----------------------------------------------------------------------------
 * Controls
 */

static int imx800_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx800 *imx800 =
		container_of(ctrl->handler, struct imx800, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&imx800->sd);
	const struct v4l2_mbus_framefmt *format;
	struct v4l2_subdev_state *state;
	int ret = 0;

	state = v4l2_subdev_get_locked_active_state(&imx800->sd);
	format = v4l2_subdev_get_pad_format(&imx800->sd, state, 0);
	dev_info(&client->dev, "imx800_set_ctrl %x\n", ctrl->id);

	if (ctrl->id == V4L2_CID_VBLANK) {
		int exposure_max, exposure_def;

		/* Update max exposure while meeting expected vblanking */
		exposure_max = format->height + ctrl->val - 4;
		exposure_def = (exposure_max < IMX800_EXPOSURE_DEFAULT) ?
			exposure_max : IMX800_EXPOSURE_DEFAULT;
		__v4l2_ctrl_modify_range(imx800->exposure,
					 imx800->exposure->minimum,
					 exposure_max, imx800->exposure->step,
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
//		cci_write(imx800->regmap, IMX800_REG_ANALOG_GAIN,
//			  ctrl->val, &ret);
		break;
	case V4L2_CID_EXPOSURE:
//		cci_write(imx800->regmap, IMX800_REG_EXPOSURE,
//			  ctrl->val, &ret);
		break;
	case V4L2_CID_DIGITAL_GAIN:
//		cci_write(imx800->regmap, IMX800_REG_DIGITAL_GAIN,
//			  ctrl->val, &ret);
		break;
	case V4L2_CID_TEST_PATTERN:
		cci_write(imx800->regmap, IMX800_REG_TEST_PATTERN,
			  imx800_test_pattern_val[ctrl->val], &ret);
		break;
	case V4L2_CID_HFLIP:
	case V4L2_CID_VFLIP:
		cci_write(imx800->regmap, IMX800_REG_ORIENTATION,
			  imx800->hflip->val | imx800->vflip->val << 1, &ret);
		break;
	case V4L2_CID_VBLANK:
//		cci_write(imx800->regmap, IMX800_REG_VTS,
//			  format->height + ctrl->val, &ret);
		break;
	case V4L2_CID_TEST_PATTERN_RED:
		cci_write(imx800->regmap, IMX800_REG_TESTP_RED,
			  ctrl->val, &ret);
		break;
	case V4L2_CID_TEST_PATTERN_GREENR:
		cci_write(imx800->regmap, IMX800_REG_TESTP_GREENR,
			  ctrl->val, &ret);
		break;
	case V4L2_CID_TEST_PATTERN_BLUE:
		cci_write(imx800->regmap, IMX800_REG_TESTP_BLUE,
			  ctrl->val, &ret);
		break;
	case V4L2_CID_TEST_PATTERN_GREENB:
		cci_write(imx800->regmap, IMX800_REG_TESTP_GREENB,
			  ctrl->val, &ret);
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

static const struct v4l2_ctrl_ops imx800_ctrl_ops = {
	.s_ctrl = imx800_set_ctrl,
};

/* Initialize control handlers */
static int imx800_init_controls(struct imx800 *imx800)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx800->sd);
	const struct imx800_mode *mode = &supported_modes[0];
	struct v4l2_ctrl_handler *ctrl_hdlr;
	struct v4l2_fwnode_device_properties props;
	int exposure_max, exposure_def, hblank;
	int i, ret;
	dev_err(&client->dev, "imx800_init_controls\n");

	ctrl_hdlr = &imx800->ctrl_handler;
	ret = v4l2_ctrl_handler_init(ctrl_hdlr, 12);
	if (ret)
		return ret;

	/* By default, PIXEL_RATE is read only */
	imx800->pixel_rate = v4l2_ctrl_new_std(ctrl_hdlr, &imx800_ctrl_ops,
					       V4L2_CID_PIXEL_RATE,
					       IMX800_PIXEL_RATE,
					       IMX800_PIXEL_RATE, 1,
					       IMX800_PIXEL_RATE);

	imx800->link_freq =
		v4l2_ctrl_new_int_menu(ctrl_hdlr, &imx800_ctrl_ops,
				       V4L2_CID_LINK_FREQ,
				       ARRAY_SIZE(imx800_link_freq_menu) - 1, 0,
				       imx800_link_freq_menu);
	if (imx800->link_freq)
		imx800->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	/* Initial vblank/hblank/exposure parameters based on current mode */
	imx800->vblank = v4l2_ctrl_new_std(ctrl_hdlr, &imx800_ctrl_ops,
					   V4L2_CID_VBLANK, IMX800_VBLANK_MIN,
					   IMX800_VTS_MAX - mode->height, 1,
					   mode->vts_def - mode->height);
	hblank = IMX800_PPL_DEFAULT - mode->width;
	imx800->hblank = v4l2_ctrl_new_std(ctrl_hdlr, &imx800_ctrl_ops,
					   V4L2_CID_HBLANK, hblank, hblank,
					   1, hblank);
	if (imx800->hblank)
		imx800->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	exposure_max = mode->vts_def - 4;
	exposure_def = (exposure_max < IMX800_EXPOSURE_DEFAULT) ?
		exposure_max : IMX800_EXPOSURE_DEFAULT;
	imx800->exposure = v4l2_ctrl_new_std(ctrl_hdlr, &imx800_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     IMX800_EXPOSURE_MIN, exposure_max,
					     IMX800_EXPOSURE_STEP,
					     exposure_def);

	v4l2_ctrl_new_std(ctrl_hdlr, &imx800_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			  IMX800_ANA_GAIN_MIN, IMX800_ANA_GAIN_MAX,
			  IMX800_ANA_GAIN_STEP, IMX800_ANA_GAIN_DEFAULT);

	v4l2_ctrl_new_std(ctrl_hdlr, &imx800_ctrl_ops, V4L2_CID_DIGITAL_GAIN,
			  IMX800_DGTL_GAIN_MIN, IMX800_DGTL_GAIN_MAX,
			  IMX800_DGTL_GAIN_STEP, IMX800_DGTL_GAIN_DEFAULT);

	imx800->hflip = v4l2_ctrl_new_std(ctrl_hdlr, &imx800_ctrl_ops,
					  V4L2_CID_HFLIP, 0, 1, 1, 0);
	if (imx800->hflip)
		imx800->hflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;

	imx800->vflip = v4l2_ctrl_new_std(ctrl_hdlr, &imx800_ctrl_ops,
					  V4L2_CID_VFLIP, 0, 1, 1, 0);
	if (imx800->vflip)
		imx800->vflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;

	v4l2_ctrl_new_std_menu_items(ctrl_hdlr, &imx800_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(imx800_test_pattern_menu) - 1,
				     0, 0, imx800_test_pattern_menu);
	for (i = 0; i < 4; i++) {
		/*
		 * The assumption is that
		 * V4L2_CID_TEST_PATTERN_GREENR == V4L2_CID_TEST_PATTERN_RED + 1
		 * V4L2_CID_TEST_PATTERN_BLUE   == V4L2_CID_TEST_PATTERN_RED + 2
		 * V4L2_CID_TEST_PATTERN_GREENB == V4L2_CID_TEST_PATTERN_RED + 3
		 */
		v4l2_ctrl_new_std(ctrl_hdlr, &imx800_ctrl_ops,
				  V4L2_CID_TEST_PATTERN_RED + i,
				  IMX800_TESTP_COLOUR_MIN,
				  IMX800_TESTP_COLOUR_MAX,
				  IMX800_TESTP_COLOUR_STEP,
				  IMX800_TESTP_COLOUR_MAX);
		/* The "Solid color" pattern is white by default */
	}

	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		dev_err(&client->dev, "%s control init failed (%d)\n",
			__func__, ret);
		goto error;
	}

	ret = v4l2_fwnode_device_parse(&client->dev, &props);
	if (ret)
		goto error;

	ret = v4l2_ctrl_new_fwnode_properties(ctrl_hdlr, &imx800_ctrl_ops,
					      &props);
	if (ret)
		goto error;

	imx800->sd.ctrl_handler = ctrl_hdlr;
	dev_info(&client->dev, "imx800_init_controls ok\n");

	return 0;

error:
	v4l2_ctrl_handler_free(ctrl_hdlr);
	dev_info(&client->dev, "imx800_init_controls failed %i\n", ret);

	return ret;
}

static void imx800_free_controls(struct imx800 *imx800)
{
	v4l2_ctrl_handler_free(imx800->sd.ctrl_handler);
}

/* -----------------------------------------------------------------------------
 * Subdev operations
 */

static int imx800_start_streaming(struct imx800 *imx800,
				  struct v4l2_subdev_state *state)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx800->sd);
	const struct imx800_reg_list *reg_list;
	int ret;
	dev_info(&client->dev, "imx800_start_streaming\n");

	ret = pm_runtime_resume_and_get(&client->dev);
	if (ret < 0)
		return ret;

	/* Send all registers that are common to all modes */
	ret = cci_multi_reg_write(imx800->regmap, imx800_common_regs,
				  ARRAY_SIZE(imx800_common_regs), NULL);
	if (ret) {
		dev_err(&client->dev, "%s failed to send mfg header\n", __func__);
		goto err_rpm_put;
	}

	/* Apply default values of current mode */
	reg_list = &imx800->mode->reg_list;
	ret = cci_multi_reg_write(imx800->regmap, reg_list->regs,
				  reg_list->num_of_regs, NULL);
	if (ret) {
		dev_err(&client->dev, "%s failed to set mode\n", __func__);
		goto err_rpm_put;
	}

	/* Apply customized values from user */
	ret =  __v4l2_ctrl_handler_setup(imx800->sd.ctrl_handler);
	if (ret)
		goto err_rpm_put;

	/* set stream on register */
	ret = cci_write(imx800->regmap, IMX800_REG_MODE_SELECT,
			IMX800_MODE_STREAMING, NULL);
	if (ret)
		goto err_rpm_put;

	/* vflip and hflip cannot change during streaming */
	__v4l2_ctrl_grab(imx800->vflip, true);
	__v4l2_ctrl_grab(imx800->hflip, true);

	dev_info(&client->dev, "imx800_start_streaming ok\n");
	return 0;

err_rpm_put:
	pm_runtime_put(&client->dev);
	dev_info(&client->dev, "imx800_start_streaming failed %i\n", ret);
	return ret;
}

static void imx800_stop_streaming(struct imx800 *imx800)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx800->sd);
	int ret;
	dev_info(&client->dev, "imx800_stop_streaming\n");

	/* set stream off register */
	ret = cci_write(imx800->regmap, IMX800_REG_MODE_SELECT,
			IMX800_MODE_STANDBY, NULL);
	if (ret)
		dev_err(&client->dev, "%s failed to set stream\n", __func__);

	__v4l2_ctrl_grab(imx800->vflip, false);
	__v4l2_ctrl_grab(imx800->hflip, false);

	pm_runtime_put(&client->dev);
}

static int imx800_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct imx800 *imx800 = to_imx800(sd);
	struct v4l2_subdev_state *state;
	int ret = 0;
	dev_info(sd->dev, "imx800_set_stream\n");

	state = v4l2_subdev_lock_and_get_active_state(sd);

	if (enable)
		ret = imx800_start_streaming(imx800, state);
	else
		imx800_stop_streaming(imx800);

	v4l2_subdev_unlock_state(state);
	return ret;
}

static void imx800_update_pad_format(struct imx800 *imx800,
				     const struct imx800_mode *mode,
				     struct v4l2_mbus_framefmt *fmt, u32 code)
{
	/* Bayer order varies with flips */
	fmt->code = imx800_get_format_code(imx800, code);
	fmt->width = mode->width;
	fmt->height = mode->height;
	fmt->field = V4L2_FIELD_NONE;
	fmt->colorspace = V4L2_COLORSPACE_RAW;
	fmt->ycbcr_enc = V4L2_YCBCR_ENC_601;
	fmt->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	fmt->xfer_func = V4L2_XFER_FUNC_NONE;
}

static int imx800_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct imx800 *imx800 = to_imx800(sd);
	dev_info(sd->dev, "imx800_enum_mbus_code\n");

	if (code->index >= (ARRAY_SIZE(imx800_mbus_formats) / 4))
		return -EINVAL;

	code->code = imx800_get_format_code(imx800, imx800_mbus_formats[code->index * 4]);

	return 0;
}

static int imx800_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *state,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	struct imx800 *imx800 = to_imx800(sd);
	u32 code;
	dev_info(sd->dev, "imx800_enum_frame_size\n");

	if (fse->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	code = imx800_get_format_code(imx800, fse->code);
	if (fse->code != code)
		return -EINVAL;

	fse->min_width = supported_modes[fse->index].width;
	fse->max_width = fse->min_width;
	fse->min_height = supported_modes[fse->index].height;
	fse->max_height = fse->min_height;

	return 0;
}

static int imx800_set_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state,
				 struct v4l2_subdev_format *fmt)
{
	struct imx800 *imx800 = to_imx800(sd);
	const struct imx800_mode *mode;
	struct v4l2_mbus_framefmt *format;
	struct v4l2_rect *crop;
	unsigned int bin_h, bin_v;
	dev_info(sd->dev, "imx800_set_pad_format\n");

	mode = v4l2_find_nearest_size(supported_modes,
				      ARRAY_SIZE(supported_modes),
				      width, height,
				      fmt->format.width, fmt->format.height);

	imx800_update_pad_format(imx800, mode, &fmt->format, fmt->format.code);

	format = v4l2_subdev_get_pad_format(sd, state, 0);
	*format = fmt->format;

	/*
	 * Use binning to maximize the crop rectangle size, and centre it in the
	 * sensor.
	 */
	bin_h = min(IMX800_PIXEL_ARRAY_WIDTH / format->width, 2U);
	bin_v = min(IMX800_PIXEL_ARRAY_HEIGHT / format->height, 2U);

	crop = v4l2_subdev_get_pad_crop(sd, state, 0);
	crop->width = format->width * bin_h;
	crop->height = format->height * bin_v;
	crop->left = (IMX800_NATIVE_WIDTH - crop->width) / 2;
	crop->top = (IMX800_NATIVE_HEIGHT - crop->height) / 2;

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		int exposure_max;
		int exposure_def;
		int hblank;

		imx800->mode = mode;

		/* Update limits and set FPS to default */
		__v4l2_ctrl_modify_range(imx800->vblank, IMX800_VBLANK_MIN,
					 IMX800_VTS_MAX - mode->height, 1,
					 mode->vts_def - mode->height);
		__v4l2_ctrl_s_ctrl(imx800->vblank,
				   mode->vts_def - mode->height);
		/* Update max exposure while meeting expected vblanking */
		exposure_max = mode->vts_def - 4;
		exposure_def = (exposure_max < IMX800_EXPOSURE_DEFAULT) ?
			exposure_max : IMX800_EXPOSURE_DEFAULT;
		__v4l2_ctrl_modify_range(imx800->exposure,
					 imx800->exposure->minimum,
					 exposure_max, imx800->exposure->step,
					 exposure_def);
		/*
		 * Currently PPL is fixed to IMX800_PPL_DEFAULT, so hblank
		 * depends on mode->width only, and is not changeble in any
		 * way other than changing the mode.
		 */
		hblank = IMX800_PPL_DEFAULT - mode->width;
		__v4l2_ctrl_modify_range(imx800->hblank, hblank, hblank, 1,
					 hblank);
	}

	return 0;
}

static int imx800_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *state,
				struct v4l2_subdev_selection *sel)
{
	dev_info(sd->dev, "imx800_get_selection\n");
	switch (sel->target) {
	case V4L2_SEL_TGT_CROP: {
		sel->r = *v4l2_subdev_get_pad_crop(sd, state, 0);
		return 0;
	}

	case V4L2_SEL_TGT_NATIVE_SIZE:
		sel->r.top = 0;
		sel->r.left = 0;
		sel->r.width = IMX800_NATIVE_WIDTH;
		sel->r.height = IMX800_NATIVE_HEIGHT;

		return 0;

	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_CROP_BOUNDS:
		sel->r.top = IMX800_PIXEL_ARRAY_TOP;
		sel->r.left = IMX800_PIXEL_ARRAY_LEFT;
		sel->r.width = IMX800_PIXEL_ARRAY_WIDTH;
		sel->r.height = IMX800_PIXEL_ARRAY_HEIGHT;

		return 0;
	}

	return -EINVAL;
}

static int imx800_init_cfg(struct v4l2_subdev *sd,
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

	imx800_set_pad_format(sd, state, &fmt);

	return 0;
}

static const struct v4l2_subdev_core_ops imx800_core_ops = {
	.subscribe_event = v4l2_ctrl_subdev_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_video_ops imx800_video_ops = {
	.s_stream = imx800_set_stream,
};

static const struct v4l2_subdev_pad_ops imx800_pad_ops = {
	.init_cfg = imx800_init_cfg,
	.enum_mbus_code = imx800_enum_mbus_code,
	.get_fmt = v4l2_subdev_get_fmt,
	.set_fmt = imx800_set_pad_format,
	.get_selection = imx800_get_selection,
	.enum_frame_size = imx800_enum_frame_size,
};

static const struct v4l2_subdev_ops imx800_subdev_ops = {
	.core = &imx800_core_ops,
	.video = &imx800_video_ops,
	.pad = &imx800_pad_ops,
};


/* -----------------------------------------------------------------------------
 * Power management
 */

static int imx800_power_on(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct imx800 *imx800 = to_imx800(sd);
	int ret;

	dev_info(dev, "imx800_power_on\n");
	ret = regulator_bulk_enable(IMX800_NUM_SUPPLIES,
				    imx800->supplies);
	if (ret) {
		dev_err(dev, "%s: failed to enable regulators\n",
			__func__);
		return ret;
	}

	ret = clk_prepare_enable(imx800->xclk);
	if (ret) {
		dev_err(dev, "%s: failed to enable clock\n",
			__func__);
		goto reg_off;
	}

	gpiod_set_value_cansleep(imx800->reset_gpio, 0);
	usleep_range(IMX800_XCLR_MIN_DELAY_US,
		     IMX800_XCLR_MIN_DELAY_US + IMX800_XCLR_DELAY_RANGE_US);

	return 0;

reg_off:
	regulator_bulk_disable(IMX800_NUM_SUPPLIES, imx800->supplies);

	return ret;
}

static int imx800_power_off(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct imx800 *imx800 = to_imx800(sd);
	dev_info(dev, "imx800_power_off\n");

	gpiod_set_value_cansleep(imx800->reset_gpio, 1);
	regulator_bulk_disable(IMX800_NUM_SUPPLIES, imx800->supplies);
	clk_disable_unprepare(imx800->xclk);

	return 0;
}

/* -----------------------------------------------------------------------------
 * Probe & remove
 */

static int imx800_get_regulators(struct imx800 *imx800)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx800->sd);
	unsigned int i;

	for (i = 0; i < IMX800_NUM_SUPPLIES; i++)
		imx800->supplies[i].supply = imx800_supply_name[i];

	return devm_regulator_bulk_get(&client->dev,
				       IMX800_NUM_SUPPLIES,
				       imx800->supplies);
}

/* Verify chip ID */
static int imx800_identify_module(struct imx800 *imx800)
{
	struct i2c_client *client = v4l2_get_subdevdata(&imx800->sd);
	int ret;
	u64 val;

	ret = cci_read(imx800->regmap, IMX800_REG_CHIP_ID, &val, NULL);
	if (ret) {
		dev_err(&client->dev, "failed to read chip id %x\n",
			IMX800_CHIP_ID);
		return ret;
	}

	if (val != IMX800_CHIP_ID) {
		dev_err(&client->dev, "chip id mismatch: %x!=%llx\n",
			IMX800_CHIP_ID, val);
		return -EIO;
	}

	return 0;
}

static int imx800_check_hwcfg(struct device *dev, struct imx800 *imx800)
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
/*
	if (ep_cfg.bus.mipi_csi2.num_data_lanes != 4) {
		dev_err(dev, "only 4 data lanes are currently supported\n");
		goto error_out;
	}
*/
	/* Check the link frequency set in device tree */
	if (!ep_cfg.nr_of_link_frequencies) {
		dev_err(dev, "link-frequency property not found in DT\n");
		goto error_out;
	}

	if (ep_cfg.nr_of_link_frequencies != 1 ||
	   (ep_cfg.link_frequencies[0] != IMX800_DEFAULT_LINK_FREQ)) {
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

static int imx800_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct imx800 *imx800;
	int ret;

	imx800 = devm_kzalloc(&client->dev, sizeof(*imx800), GFP_KERNEL);
	if (!imx800)
		return -ENOMEM;

	v4l2_i2c_subdev_init(&imx800->sd, client, &imx800_subdev_ops);

	/* Check the hardware configuration in device tree */
	if (imx800_check_hwcfg(dev, imx800))
		return -EINVAL;

	imx800->regmap = devm_cci_regmap_init_i2c(client, 16);
	if (IS_ERR(imx800->regmap)) {
		ret = PTR_ERR(imx800->regmap);
		dev_err(dev, "failed to initialize CCI: %d\n", ret);
		return ret;
	}

	/* Get system clock (xclk) */
	imx800->xclk = devm_clk_get(dev, NULL);
	if (IS_ERR(imx800->xclk)) {
		dev_err(dev, "failed to get xclk\n");
		return PTR_ERR(imx800->xclk);
	}

	imx800->xclk_freq = clk_get_rate(imx800->xclk);
	if (imx800->xclk_freq != IMX800_XCLK_FREQ) {
		dev_err(dev, "xclk frequency not supported: %d Hz\n",
			imx800->xclk_freq);
		return -EINVAL;
	}

	ret = imx800_get_regulators(imx800);
	if (ret) {
		dev_err(dev, "failed to get regulators\n");
		return ret;
	}

	/* Request optional enable pin */
	imx800->reset_gpio = devm_gpiod_get_optional(dev, "reset",
						     GPIOD_OUT_LOW);

	/*
	 * The sensor must be powered for imx800_identify_module()
	 * to be able to read the CHIP_ID register
	 */
	ret = imx800_power_on(dev);
	if (ret)
		return ret;

	ret = imx800_identify_module(imx800);
	if (ret)
		goto error_power_off;

	/* Set default mode to max resolution */
	imx800->mode = &supported_modes[0];

	/*
	 * Sensor doesn't enter LP-11 state upon power up until and unless
	 * streaming is started, so upon power up switch the modes to:
	 * streaming -> standby
	 */
	ret = cci_write(imx800->regmap, IMX800_REG_MODE_SELECT,
			IMX800_MODE_STREAMING, NULL);
	if (ret < 0)
		goto error_power_off;

	usleep_range(100, 110);

	/* put sensor back to standby mode */
	ret = cci_write(imx800->regmap, IMX800_REG_MODE_SELECT,
			IMX800_MODE_STANDBY, NULL);
	if (ret < 0)
		goto error_power_off;

	usleep_range(100, 110);

	ret = imx800_init_controls(imx800);
	if (ret)
		goto error_power_off;

	/* Initialize subdev */
	imx800->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
			    V4L2_SUBDEV_FL_HAS_EVENTS;
	imx800->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	/* Initialize source pad */
	imx800->pad.flags = MEDIA_PAD_FL_SOURCE;

	ret = media_entity_pads_init(&imx800->sd.entity, 1, &imx800->pad);
	if (ret) {
		dev_err(dev, "failed to init entity pads: %d\n", ret);
		goto error_handler_free;
	}

	imx800->sd.state_lock = imx800->ctrl_handler.lock;
	ret = v4l2_subdev_init_finalize(&imx800->sd);
	if (ret < 0) {
		dev_err(dev, "subdev init error: %d\n", ret);
		goto error_media_entity;
	}

	ret = v4l2_async_register_subdev_sensor(&imx800->sd);
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
	v4l2_subdev_cleanup(&imx800->sd);

error_media_entity:
	media_entity_cleanup(&imx800->sd.entity);

error_handler_free:
	imx800_free_controls(imx800);

error_power_off:
	imx800_power_off(dev);

	return ret;
}

static void imx800_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx800 *imx800 = to_imx800(sd);

	v4l2_async_unregister_subdev(sd);
	v4l2_subdev_cleanup(sd);
	media_entity_cleanup(&sd->entity);
	imx800_free_controls(imx800);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		imx800_power_off(&client->dev);
	pm_runtime_set_suspended(&client->dev);
}

static const struct of_device_id imx800_dt_ids[] = {
	{ .compatible = "sony,imx800" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx800_dt_ids);

static const struct dev_pm_ops imx800_pm_ops = {
	SET_RUNTIME_PM_OPS(imx800_power_off, imx800_power_on, NULL)
};

static struct i2c_driver imx800_i2c_driver = {
	.driver = {
		.name = "imx800",
		.of_match_table	= imx800_dt_ids,
		.pm = &imx800_pm_ops,
	},
	.probe = imx800_probe,
	.remove = imx800_remove,
};

module_i2c_driver(imx800_i2c_driver);

MODULE_DESCRIPTION("Sony IMX800 sensor driver");
MODULE_LICENSE("GPL v2");
