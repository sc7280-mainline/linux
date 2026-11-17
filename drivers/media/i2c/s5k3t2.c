// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung S5K3T2 20MP image sensor driver
 *
 * Register tables from the sensor module data of the Xiaomi 11 Lite 5G NE
 * (lisa) stock firmware. Based on the S5KGW3 driver.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/units.h>
#include <media/v4l2-cci.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>

#define S5K3T2_MCLK_FREQ		19200000
#define S5K3T2_DATA_LANES		4

/*
 * Video timing pixel rate: 19.2 MHz / 3 (0x0304) * 185 (0x0306) / 7 (0x0300)
 * * 4 pixels per clock. It matches line length * frame length * frame rate
 * of the stock mode (10880 * 2064 * 30.13).
 */
#define S5K3T2_PIXEL_RATE		676571428

/* 19.2 MHz / 3 (0x030e) * 194 (0x0310) >> 2 (0x0312) */
#define S5K3T2_LINK_FREQ		310400000

#define S5K3T2_REG_CHIP_ID		CCI_REG16(0x0000)
#define S5K3T2_CHIP_ID			0x3142

#define S5K3T2_REG_CTRL_MODE		CCI_REG8(0x0100)
#define S5K3T2_MODE_STREAMING		BIT(0)

/*
 * The stock firmware streams with both flips set (0x0100 = 0x0103), so the
 * controls are applied on top of that.
 */
#define S5K3T2_REG_ORIENTATION		CCI_REG8(0x0101)
#define S5K3T2_VFLIP			BIT(1)
#define S5K3T2_HFLIP			BIT(0)
#define S5K3T2_ORIENTATION_DEFAULT	(S5K3T2_VFLIP | S5K3T2_HFLIP)

#define S5K3T2_REG_EXPOSURE		CCI_REG16(0x0202)
#define S5K3T2_EXPOSURE_MIN		6
#define S5K3T2_EXPOSURE_STEP		1
#define S5K3T2_EXPOSURE_MARGIN		16

/* Analogue gain code, 0x20 is 1x */
#define S5K3T2_REG_AGAIN		CCI_REG16(0x0204)
#define S5K3T2_AGAIN_MIN		0x20
#define S5K3T2_AGAIN_MAX		0x200
#define S5K3T2_AGAIN_STEP		1
#define S5K3T2_AGAIN_DEFAULT		0x40

#define S5K3T2_REG_VTS			CCI_REG16(0x0340)
#define S5K3T2_VTS_MAX			0xffff

#define S5K3T2_REG_TEST_PATTERN		CCI_REG16(0x0600)

/*
 * The stock mode reads 5184x3880 starting at (8, 8); the array is assumed
 * to have the same margin on every side.
 */
#define S5K3T2_NATIVE_WIDTH		5200
#define S5K3T2_NATIVE_HEIGHT		3896

#define to_s5k3t2(_sd)			container_of(_sd, struct s5k3t2, sd)

static const s64 s5k3t2_link_freq_menu[] = {
	S5K3T2_LINK_FREQ,
};

/*
 * Indexed by the flip controls: vflip << 1 | hflip. Assumes a native GRBG
 * array, which reads out as GBRG with both sensor flips set.
 */
static const u32 s5k3t2_mbus_formats[] = {
	MEDIA_BUS_FMT_SGBRG10_1X10,	MEDIA_BUS_FMT_SBGGR10_1X10,
	MEDIA_BUS_FMT_SRGGB10_1X10,	MEDIA_BUS_FMT_SGRBG10_1X10,
};

static const char * const s5k3t2_supply_names[] = {
	"dovdd",
	"avdd",
	"dvdd",
};

static const char * const s5k3t2_test_pattern_menu[] = {
	"Disabled",
	"Solid color",
	"Color bars",
	"Fade to grey color bars",
	"PN9",
};

static const struct cci_reg_sequence s5k3t2_init_reset[] = {
	{ CCI_REG16(0x0100), 0x0003 },
	{ CCI_REG16(0x6028), 0x4000 },
	{ CCI_REG16(0x6010), 0x0001 },
};

static const struct cci_reg_sequence s5k3t2_init[] = {
	{ CCI_REG16(0x6214), 0xff7d },
	{ CCI_REG16(0x6218), 0x0000 },
	{ CCI_REG16(0x0a02), 0x003f },
	{ CCI_REG16(0x6028), 0x2000 },
	{ CCI_REG16(0x602a), 0x3aec },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0x0549 },
	{ CCI_REG16(0x6f12), 0x0448 },
	{ CCI_REG16(0x6f12), 0x054a },
	{ CCI_REG16(0x6f12), 0xc1f8 },
	{ CCI_REG16(0x6f12), 0x2c05 },
	{ CCI_REG16(0x6f12), 0x101a },
	{ CCI_REG16(0x6f12), 0xa1f8 },
	{ CCI_REG16(0x6f12), 0x3005 },
	{ CCI_REG16(0x6f12), 0x00f0 },
	{ CCI_REG16(0x6f12), 0x7bb8 },
	{ CCI_REG16(0x6f12), 0x2000 },
	{ CCI_REG16(0x6f12), 0x3ca0 },
	{ CCI_REG16(0x6f12), 0x2000 },
	{ CCI_REG16(0x6f12), 0x2670 },
	{ CCI_REG16(0x6f12), 0x2000 },
	{ CCI_REG16(0x6f12), 0x9c00 },
	{ CCI_REG16(0x6f12), 0x70b5 },
	{ CCI_REG16(0x6f12), 0x0646 },
	{ CCI_REG16(0x6f12), 0x4348 },
	{ CCI_REG16(0x6f12), 0x0022 },
	{ CCI_REG16(0x6f12), 0x0168 },
	{ CCI_REG16(0x6f12), 0x0c0c },
	{ CCI_REG16(0x6f12), 0x8db2 },
	{ CCI_REG16(0x6f12), 0x2946 },
	{ CCI_REG16(0x6f12), 0x2046 },
	{ CCI_REG16(0x6f12), 0x00f0 },
	{ CCI_REG16(0x6f12), 0x9bf8 },
	{ CCI_REG16(0x6f12), 0x3046 },
	{ CCI_REG16(0x6f12), 0x00f0 },
	{ CCI_REG16(0x6f12), 0x9df8 },
	{ CCI_REG16(0x6f12), 0x3d48 },
	{ CCI_REG16(0x6f12), 0x3e4a },
	{ CCI_REG16(0x6f12), 0x0830 },
	{ CCI_REG16(0x6f12), 0x0188 },
	{ CCI_REG16(0x6f12), 0x1180 },
	{ CCI_REG16(0x6f12), 0x911c },
	{ CCI_REG16(0x6f12), 0x4088 },
	{ CCI_REG16(0x6f12), 0x0880 },
	{ CCI_REG16(0x6f12), 0x2946 },
	{ CCI_REG16(0x6f12), 0x2046 },
	{ CCI_REG16(0x6f12), 0xbde8 },
	{ CCI_REG16(0x6f12), 0x7040 },
	{ CCI_REG16(0x6f12), 0x0122 },
	{ CCI_REG16(0x6f12), 0x00f0 },
	{ CCI_REG16(0x6f12), 0x89b8 },
	{ CCI_REG16(0x6f12), 0x70b5 },
	{ CCI_REG16(0x6f12), 0x0646 },
	{ CCI_REG16(0x6f12), 0x3548 },
	{ CCI_REG16(0x6f12), 0x0022 },
	{ CCI_REG16(0x6f12), 0x4068 },
	{ CCI_REG16(0x6f12), 0x84b2 },
	{ CCI_REG16(0x6f12), 0x050c },
	{ CCI_REG16(0x6f12), 0x2146 },
	{ CCI_REG16(0x6f12), 0x2846 },
	{ CCI_REG16(0x6f12), 0x00f0 },
	{ CCI_REG16(0x6f12), 0x7ef8 },
	{ CCI_REG16(0x6f12), 0x3046 },
	{ CCI_REG16(0x6f12), 0x00f0 },
	{ CCI_REG16(0x6f12), 0x85f8 },
	{ CCI_REG16(0x6f12), 0x3149 },
	{ CCI_REG16(0x6f12), 0x314b },
	{ CCI_REG16(0x6f12), 0x0120 },
	{ CCI_REG16(0x6f12), 0x0988 },
	{ CCI_REG16(0x6f12), 0x40ea },
	{ CCI_REG16(0x6f12), 0x0110 },
	{ CCI_REG16(0x6f12), 0x5881 },
	{ CCI_REG16(0x6f12), 0x2f48 },
	{ CCI_REG16(0x6f12), 0x0078 },
	{ CCI_REG16(0x6f12), 0x68b1 },
	{ CCI_REG16(0x6f12), 0x0022 },
	{ CCI_REG16(0x6f12), 0x2e48 },
	{ CCI_REG16(0x6f12), 0x2f49 },
	{ CCI_REG16(0x6f12), 0x80f8 },
	{ CCI_REG16(0x6f12), 0xc428 },
	{ CCI_REG16(0x6f12), 0x0a80 },
	{ CCI_REG16(0x6f12), 0x2e49 },
	{ CCI_REG16(0x6f12), 0x0e78 },
	{ CCI_REG16(0x6f12), 0x36b1 },
	{ CCI_REG16(0x6f12), 0x90f8 },
	{ CCI_REG16(0x6f12), 0xe803 },
	{ CCI_REG16(0x6f12), 0x18b1 },
	{ CCI_REG16(0x6f12), 0x0120 },
	{ CCI_REG16(0x6f12), 0x02e0 },
	{ CCI_REG16(0x6f12), 0x0122 },
	{ CCI_REG16(0x6f12), 0xf0e7 },
	{ CCI_REG16(0x6f12), 0x0020 },
	{ CCI_REG16(0x6f12), 0x4978 },
	{ CCI_REG16(0x6f12), 0x01b1 },
	{ CCI_REG16(0x6f12), 0x42b1 },
	{ CCI_REG16(0x6f12), 0x0021 },
	{ CCI_REG16(0x6f12), 0x40ea },
	{ CCI_REG16(0x6f12), 0x0110 },
	{ CCI_REG16(0x6f12), 0x5880 },
	{ CCI_REG16(0x6f12), 0x00f0 },
	{ CCI_REG16(0x6f12), 0x66f8 },
	{ CCI_REG16(0x6f12), 0x0128 },
	{ CCI_REG16(0x6f12), 0x02d0 },
	{ CCI_REG16(0x6f12), 0x1be0 },
	{ CCI_REG16(0x6f12), 0x0121 },
	{ CCI_REG16(0x6f12), 0xf5e7 },
	{ CCI_REG16(0x6f12), 0x2248 },
	{ CCI_REG16(0x6f12), 0x234a },
	{ CCI_REG16(0x6f12), 0x234e },
	{ CCI_REG16(0x6f12), 0xb0f8 },
	{ CCI_REG16(0x6f12), 0x7211 },
	{ CCI_REG16(0x6f12), 0x92f8 },
	{ CCI_REG16(0x6f12), 0x9420 },
	{ CCI_REG16(0x6f12), 0x90f8 },
	{ CCI_REG16(0x6f12), 0x7401 },
	{ CCI_REG16(0x6f12), 0xd140 },
	{ CCI_REG16(0x6f12), 0xd040 },
	{ CCI_REG16(0x6f12), 0x4318 },
	{ CCI_REG16(0x6f12), 0x96f8 },
	{ CCI_REG16(0x6f12), 0x7f63 },
	{ CCI_REG16(0x6f12), 0x581e },
	{ CCI_REG16(0x6f12), 0x022e },
	{ CCI_REG16(0x6f12), 0x03d9 },
	{ CCI_REG16(0x6f12), 0x0220 },
	{ CCI_REG16(0x6f12), 0x9040 },
	{ CCI_REG16(0x6f12), 0x181a },
	{ CCI_REG16(0x6f12), 0x401c },
	{ CCI_REG16(0x6f12), 0x164a },
	{ CCI_REG16(0x6f12), 0x703a },
	{ CCI_REG16(0x6f12), 0x1180 },
	{ CCI_REG16(0x6f12), 0x911c },
	{ CCI_REG16(0x6f12), 0x0880 },
	{ CCI_REG16(0x6f12), 0x2146 },
	{ CCI_REG16(0x6f12), 0x2846 },
	{ CCI_REG16(0x6f12), 0xbde8 },
	{ CCI_REG16(0x6f12), 0x7040 },
	{ CCI_REG16(0x6f12), 0x0122 },
	{ CCI_REG16(0x6f12), 0x00f0 },
	{ CCI_REG16(0x6f12), 0x31b8 },
	{ CCI_REG16(0x6f12), 0x10b5 },
	{ CCI_REG16(0x6f12), 0x0022 },
	{ CCI_REG16(0x6f12), 0xaff2 },
	{ CCI_REG16(0x6f12), 0xb501 },
	{ CCI_REG16(0x6f12), 0x1348 },
	{ CCI_REG16(0x6f12), 0x00f0 },
	{ CCI_REG16(0x6f12), 0x3ef8 },
	{ CCI_REG16(0x6f12), 0x064c },
	{ CCI_REG16(0x6f12), 0x0022 },
	{ CCI_REG16(0x6f12), 0xaff2 },
	{ CCI_REG16(0x6f12), 0xff01 },
	{ CCI_REG16(0x6f12), 0x6060 },
	{ CCI_REG16(0x6f12), 0x1048 },
	{ CCI_REG16(0x6f12), 0x00f0 },
	{ CCI_REG16(0x6f12), 0x36f8 },
	{ CCI_REG16(0x6f12), 0x0f49 },
	{ CCI_REG16(0x6f12), 0x2060 },
	{ CCI_REG16(0x6f12), 0x7a20 },
	{ CCI_REG16(0x6f12), 0x0968 },
	{ CCI_REG16(0x6f12), 0x4883 },
	{ CCI_REG16(0x6f12), 0x10bd },
	{ CCI_REG16(0x6f12), 0x2000 },
	{ CCI_REG16(0x6f12), 0x3c90 },
	{ CCI_REG16(0x6f12), 0x4000 },
	{ CCI_REG16(0x6f12), 0x950c },
	{ CCI_REG16(0x6f12), 0x2000 },
	{ CCI_REG16(0x6f12), 0x16f0 },
	{ CCI_REG16(0x6f12), 0x4000 },
	{ CCI_REG16(0x6f12), 0xd000 },
	{ CCI_REG16(0x6f12), 0x2000 },
	{ CCI_REG16(0x6f12), 0x19a0 },
	{ CCI_REG16(0x6f12), 0x2000 },
	{ CCI_REG16(0x6f12), 0x30c0 },
	{ CCI_REG16(0x6f12), 0x4000 },
	{ CCI_REG16(0x6f12), 0x9800 },
	{ CCI_REG16(0x6f12), 0x2000 },
	{ CCI_REG16(0x6f12), 0x1dd0 },
	{ CCI_REG16(0x6f12), 0x2000 },
	{ CCI_REG16(0x6f12), 0x17c0 },
	{ CCI_REG16(0x6f12), 0x2000 },
	{ CCI_REG16(0x6f12), 0x2210 },
	{ CCI_REG16(0x6f12), 0x2000 },
	{ CCI_REG16(0x6f12), 0x2670 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0xf45f },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6f12), 0xd957 },
	{ CCI_REG16(0x6f12), 0x2000 },
	{ CCI_REG16(0x6f12), 0x08c0 },
	{ CCI_REG16(0x6f12), 0x49f6 },
	{ CCI_REG16(0x6f12), 0x213c },
	{ CCI_REG16(0x6f12), 0xc0f2 },
	{ CCI_REG16(0x6f12), 0x000c },
	{ CCI_REG16(0x6f12), 0x6047 },
	{ CCI_REG16(0x6f12), 0x4df6 },
	{ CCI_REG16(0x6f12), 0x571c },
	{ CCI_REG16(0x6f12), 0xc0f2 },
	{ CCI_REG16(0x6f12), 0x000c },
	{ CCI_REG16(0x6f12), 0x6047 },
	{ CCI_REG16(0x6f12), 0x4ff2 },
	{ CCI_REG16(0x6f12), 0x5f4c },
	{ CCI_REG16(0x6f12), 0xc0f2 },
	{ CCI_REG16(0x6f12), 0x000c },
	{ CCI_REG16(0x6f12), 0x6047 },
	{ CCI_REG16(0x6f12), 0x44f2 },
	{ CCI_REG16(0x6f12), 0x9b0c },
	{ CCI_REG16(0x6f12), 0xc0f2 },
	{ CCI_REG16(0x6f12), 0x000c },
	{ CCI_REG16(0x6f12), 0x6047 },
	{ CCI_REG16(0x6f12), 0x4bf2 },
	{ CCI_REG16(0x6f12), 0xed0c },
	{ CCI_REG16(0x6f12), 0xc0f2 },
	{ CCI_REG16(0x6f12), 0x000c },
	{ CCI_REG16(0x6f12), 0x6047 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6028), 0x2000 },
	{ CCI_REG16(0x602a), 0x1014 },
	{ CCI_REG16(0x6f12), 0x0100 },
	{ CCI_REG16(0x6f12), 0x7000 },
	{ CCI_REG16(0x602a), 0x108a },
	{ CCI_REG16(0x6f12), 0x0006 },
	{ CCI_REG16(0x602a), 0x1092 },
	{ CCI_REG16(0x6f12), 0x0005 },
	{ CCI_REG16(0x602a), 0x1096 },
	{ CCI_REG16(0x6f12), 0x0002 },
	{ CCI_REG16(0x6f12), 0x001a },
	{ CCI_REG16(0x602a), 0x109c },
	{ CCI_REG16(0x6f12), 0x0014 },
	{ CCI_REG16(0x602a), 0x10a2 },
	{ CCI_REG16(0x6f12), 0x0022 },
	{ CCI_REG16(0x602a), 0x10ae },
	{ CCI_REG16(0x6f12), 0x0007 },
	{ CCI_REG16(0x602a), 0x10c2 },
	{ CCI_REG16(0x6f12), 0x001e },
	{ CCI_REG16(0x602a), 0x10f4 },
	{ CCI_REG16(0x6f12), 0x0003 },
	{ CCI_REG16(0x6f12), 0x0003 },
	{ CCI_REG16(0x602a), 0x110a },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x113e },
	{ CCI_REG16(0x6f12), 0x0001 },
	{ CCI_REG16(0x6f12), 0x014e },
	{ CCI_REG16(0x602a), 0x13ea },
	{ CCI_REG16(0x6f12), 0x160f },
	{ CCI_REG16(0x6f12), 0x0d00 },
	{ CCI_REG16(0x602a), 0x13fa },
	{ CCI_REG16(0x6f12), 0x009d },
	{ CCI_REG16(0x6f12), 0x0107 },
	{ CCI_REG16(0x602a), 0x14e2 },
	{ CCI_REG16(0x6f12), 0x04c2 },
	{ CCI_REG16(0x6f12), 0x02ae },
	{ CCI_REG16(0x6f12), 0x0020 },
	{ CCI_REG16(0x6028), 0x4000 },
	{ CCI_REG16(0xf44a), 0x0010 },
	{ CCI_REG16(0xf46a), 0xb6a0 },
	{ CCI_REG16(0x6028), 0x2000 },
	{ CCI_REG16(0x602a), 0x0f5e },
	{ CCI_REG16(0x6f12), 0x0200 },
	{ CCI_REG16(0x602a), 0x0f90 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x602a), 0x17c0 },
	{ CCI_REG16(0x6f12), 0x0010 },
	{ CCI_REG16(0x6f12), 0x0201 },
	{ CCI_REG16(0x602a), 0x1da2 },
	{ CCI_REG16(0x6f12), 0x0001 },
	{ CCI_REG16(0x6f12), 0x0203 },
	{ CCI_REG16(0x6f12), 0x0405 },
	{ CCI_REG16(0x6f12), 0x0607 },
	{ CCI_REG16(0x6f12), 0x0809 },
	{ CCI_REG16(0x6f12), 0x0a0b },
	{ CCI_REG16(0x6f12), 0x0c0d },
	{ CCI_REG16(0x6f12), 0x0e0f },
	{ CCI_REG16(0x6028), 0x4000 },
	{ CCI_REG16(0x020c), 0x0001 },
	{ CCI_REG16(0x0bc6), 0x0000 },
	{ CCI_REG16(0x0d00), 0x0000 },
	{ CCI_REG16(0xb13c), 0x0800 },
};

static const struct cci_reg_sequence s5k3t2_2592x1940_mode[] = {
	{ CCI_REG16(0x6028), 0x4000 },
	{ CCI_REG16(0x0136), 0x1300 },
	{ CCI_REG16(0x013e), 0x00c8 },
	{ CCI_REG16(0x0304), 0x0003 },
	{ CCI_REG16(0x0306), 0x00b9 },
	{ CCI_REG16(0x6028), 0x2000 },
	{ CCI_REG16(0x602a), 0x0e0e },
	{ CCI_REG16(0x6f12), 0x0105 },
	{ CCI_REG16(0x6f12), 0x0000 },
	{ CCI_REG16(0x6028), 0x4000 },
	{ CCI_REG16(0x030c), 0x0000 },
	{ CCI_REG16(0x0302), 0x0001 },
	{ CCI_REG16(0x0300), 0x0007 },
	{ CCI_REG16(0x030e), 0x0003 },
	{ CCI_REG16(0x0310), 0x00c2 },
	{ CCI_REG16(0x0312), 0x0002 },
	{ CCI_REG16(0x0308), 0x0008 },
	{ CCI_REG16(0x030a), 0x0001 },
	{ CCI_REG16(0x0344), 0x0008 },
	{ CCI_REG16(0x0346), 0x0008 },
	{ CCI_REG16(0x0348), 0x1447 },
	{ CCI_REG16(0x034a), 0x0f2f },
	{ CCI_REG16(0x034c), 0x0a20 },
	{ CCI_REG16(0x034e), 0x0794 },
	{ CCI_REG16(0x0350), 0x0000 },
	{ CCI_REG16(0x0352), 0x0000 },
	{ CCI_REG16(0x0900), 0x0122 },
	{ CCI_REG16(0x0404), 0x1000 },
	{ CCI_REG16(0x0380), 0x0002 },
	{ CCI_REG16(0x0382), 0x0002 },
	{ CCI_REG16(0x0384), 0x0002 },
	{ CCI_REG16(0x0386), 0x0002 },
	{ CCI_REG16(0x0342), 0x2a80 },
	{ CCI_REG16(0x0340), 0x0810 },
	{ CCI_REG16(0x6028), 0x4000 },
	{ CCI_REG16(0x010c), 0x0000 },
	{ CCI_REG16(0x0114), 0x0300 },
	{ CCI_REG16(0x0116), 0x3000 },
	{ CCI_REG16(0x011a), 0x0001 },
	{ CCI_REG16(0x0118), 0x0002 },
	{ CCI_REG16(0x6028), 0x2000 },
	{ CCI_REG16(0x602a), 0x0e92 },
	{ CCI_REG16(0x6f12), 0xffff },
	{ CCI_REG16(0x6028), 0x4000 },
	{ CCI_REG16(0x0b04), 0x0001 },
	{ CCI_REG16(0x0b06), 0x0101 },
	{ CCI_REG16(0x0fea), 0x04a0 },
	{ CCI_REG16(0x6028), 0x2000 },
	{ CCI_REG16(0x602a), 0x3c98 },
	{ CCI_REG16(0x6f12), 0x052c },
	{ CCI_REG16(0x6f12), 0x0a3b },
	{ CCI_REG16(0x602a), 0x1da0 },
	{ CCI_REG16(0x6f12), 0x0010 },
	{ CCI_REG16(0x602a), 0x10ac },
	{ CCI_REG16(0x6f12), 0x0014 },
	{ CCI_REG16(0x602a), 0x1110 },
	{ CCI_REG16(0x6f12), 0x001d },
	{ CCI_REG16(0x6f12), 0x004d },
	{ CCI_REG16(0x602a), 0x13e8 },
	{ CCI_REG16(0x6f12), 0x080f },
	{ CCI_REG16(0x602a), 0x13f8 },
	{ CCI_REG16(0x6f12), 0x38c8 },
};

struct s5k3t2_reg_list {
	const struct cci_reg_sequence *regs;
	unsigned int num_regs;
};

struct s5k3t2_mode {
	u32 width;
	u32 height;
	u32 hts;
	u32 vts;
	u32 exposure;
	struct v4l2_rect crop;
	unsigned int link_freq_index;
	const struct s5k3t2_reg_list reg_list;
};

static const struct s5k3t2_mode s5k3t2_supported_modes[] = {
	{
		/* 2x2 binning of the full array, 30 fps */
		.width = 2592,
		.height = 1940,
		.hts = 10880,
		.vts = 2064,
		.exposure = 2000,
		.crop = { 8, 8, 5184, 3880 },
		.link_freq_index = 0,
		.reg_list = {
			.regs = s5k3t2_2592x1940_mode,
			.num_regs = ARRAY_SIZE(s5k3t2_2592x1940_mode),
		},
	},
};

struct s5k3t2 {
	struct device *dev;
	struct regmap *regmap;
	struct clk *mclk;
	struct gpio_desc *reset_gpio;
	struct regulator_bulk_data supplies[ARRAY_SIZE(s5k3t2_supply_names)];

	struct v4l2_subdev sd;
	struct media_pad pad;

	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *exposure;
	struct v4l2_ctrl *vflip;
	struct v4l2_ctrl *hflip;

	const struct s5k3t2_mode *mode;
};

static u32 s5k3t2_orientation(struct s5k3t2 *s5k3t2)
{
	u32 flips = (s5k3t2->vflip->val ? S5K3T2_VFLIP : 0) |
		    (s5k3t2->hflip->val ? S5K3T2_HFLIP : 0);

	return flips ^ S5K3T2_ORIENTATION_DEFAULT;
}

static int s5k3t2_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct s5k3t2 *s5k3t2 = container_of(ctrl->handler, struct s5k3t2,
					     ctrl_handler);
	const struct s5k3t2_mode *mode = s5k3t2->mode;
	s64 exposure_max;
	int ret;

	if (ctrl->id == V4L2_CID_VBLANK) {
		exposure_max = mode->height + ctrl->val - S5K3T2_EXPOSURE_MARGIN;
		__v4l2_ctrl_modify_range(s5k3t2->exposure,
					 s5k3t2->exposure->minimum,
					 exposure_max,
					 s5k3t2->exposure->step,
					 s5k3t2->exposure->default_value);
	}

	/* V4L2 controls are applied, when sensor is powered up for streaming */
	if (!pm_runtime_get_if_active(s5k3t2->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_ANALOGUE_GAIN:
		ret = cci_write(s5k3t2->regmap, S5K3T2_REG_AGAIN, ctrl->val, NULL);
		break;
	case V4L2_CID_EXPOSURE:
		ret = cci_write(s5k3t2->regmap, S5K3T2_REG_EXPOSURE, ctrl->val, NULL);
		break;
	case V4L2_CID_VBLANK:
		ret = cci_write(s5k3t2->regmap, S5K3T2_REG_VTS,
				ctrl->val + mode->height, NULL);
		break;
	case V4L2_CID_VFLIP:
	case V4L2_CID_HFLIP:
		ret = cci_write(s5k3t2->regmap, S5K3T2_REG_ORIENTATION,
				s5k3t2_orientation(s5k3t2), NULL);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = cci_write(s5k3t2->regmap, S5K3T2_REG_TEST_PATTERN,
				ctrl->val, NULL);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	pm_runtime_put(s5k3t2->dev);

	return ret;
}

static const struct v4l2_ctrl_ops s5k3t2_ctrl_ops = {
	.s_ctrl = s5k3t2_set_ctrl,
};

static int s5k3t2_init_controls(struct s5k3t2 *s5k3t2)
{
	struct v4l2_ctrl_handler *ctrl_hdlr = &s5k3t2->ctrl_handler;
	const struct s5k3t2_mode *mode = s5k3t2->mode;
	s64 hblank, vblank, exposure_max;
	struct v4l2_fwnode_device_properties props;
	int ret;

	v4l2_ctrl_handler_init(ctrl_hdlr, 9);

	s5k3t2->link_freq = v4l2_ctrl_new_int_menu(ctrl_hdlr, &s5k3t2_ctrl_ops,
					V4L2_CID_LINK_FREQ,
					ARRAY_SIZE(s5k3t2_link_freq_menu) - 1,
					mode->link_freq_index,
					s5k3t2_link_freq_menu);
	if (s5k3t2->link_freq)
		s5k3t2->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	s5k3t2->pixel_rate = v4l2_ctrl_new_std(ctrl_hdlr, &s5k3t2_ctrl_ops,
					       V4L2_CID_PIXEL_RATE,
					       S5K3T2_PIXEL_RATE,
					       S5K3T2_PIXEL_RATE, 1,
					       S5K3T2_PIXEL_RATE);

	hblank = mode->hts - mode->width;
	s5k3t2->hblank = v4l2_ctrl_new_std(ctrl_hdlr, &s5k3t2_ctrl_ops,
					   V4L2_CID_HBLANK, hblank,
					   hblank, 1, hblank);
	if (s5k3t2->hblank)
		s5k3t2->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank = mode->vts - mode->height;
	s5k3t2->vblank = v4l2_ctrl_new_std(ctrl_hdlr, &s5k3t2_ctrl_ops,
					   V4L2_CID_VBLANK, vblank,
					   S5K3T2_VTS_MAX - mode->height, 1,
					   vblank);

	v4l2_ctrl_new_std(ctrl_hdlr, &s5k3t2_ctrl_ops, V4L2_CID_ANALOGUE_GAIN,
			  S5K3T2_AGAIN_MIN, S5K3T2_AGAIN_MAX,
			  S5K3T2_AGAIN_STEP, S5K3T2_AGAIN_DEFAULT);

	exposure_max = mode->vts - S5K3T2_EXPOSURE_MARGIN;
	s5k3t2->exposure = v4l2_ctrl_new_std(ctrl_hdlr, &s5k3t2_ctrl_ops,
					     V4L2_CID_EXPOSURE,
					     S5K3T2_EXPOSURE_MIN,
					     exposure_max,
					     S5K3T2_EXPOSURE_STEP,
					     mode->exposure);

	v4l2_ctrl_new_std_menu_items(ctrl_hdlr, &s5k3t2_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(s5k3t2_test_pattern_menu) - 1,
				     0, 0, s5k3t2_test_pattern_menu);

	s5k3t2->hflip = v4l2_ctrl_new_std(ctrl_hdlr, &s5k3t2_ctrl_ops,
					  V4L2_CID_HFLIP, 0, 1, 1, 0);
	if (s5k3t2->hflip)
		s5k3t2->hflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;

	s5k3t2->vflip = v4l2_ctrl_new_std(ctrl_hdlr, &s5k3t2_ctrl_ops,
					  V4L2_CID_VFLIP, 0, 1, 1, 0);
	if (s5k3t2->vflip)
		s5k3t2->vflip->flags |= V4L2_CTRL_FLAG_MODIFY_LAYOUT;

	if (ctrl_hdlr->error) {
		ret = ctrl_hdlr->error;
		goto error_free_hdlr;
	}

	ret = v4l2_fwnode_device_parse(s5k3t2->dev, &props);
	if (ret)
		goto error_free_hdlr;

	ret = v4l2_ctrl_new_fwnode_properties(ctrl_hdlr, &s5k3t2_ctrl_ops,
					      &props);
	if (ret)
		goto error_free_hdlr;

	s5k3t2->sd.ctrl_handler = ctrl_hdlr;

	return 0;

error_free_hdlr:
	v4l2_ctrl_handler_free(ctrl_hdlr);

	return ret;
}

static int s5k3t2_write_settings(struct s5k3t2 *s5k3t2)
{
	const struct s5k3t2_reg_list *reg_list = &s5k3t2->mode->reg_list;
	int ret = 0;

	cci_multi_reg_write(s5k3t2->regmap, s5k3t2_init_reset,
			    ARRAY_SIZE(s5k3t2_init_reset), &ret);
	if (ret)
		return ret;

	/* The stock data asks for 3 us after the soft reset; be generous */
	usleep_range(5 * USEC_PER_MSEC, 6 * USEC_PER_MSEC);

	cci_multi_reg_write(s5k3t2->regmap, s5k3t2_init,
			    ARRAY_SIZE(s5k3t2_init), &ret);
	cci_multi_reg_write(s5k3t2->regmap, reg_list->regs,
			    reg_list->num_regs, &ret);

	return ret;
}

static int s5k3t2_enable_streams(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state, u32 pad,
				 u64 streams_mask)
{
	struct s5k3t2 *s5k3t2 = to_s5k3t2(sd);
	int ret;

	ret = pm_runtime_resume_and_get(s5k3t2->dev);
	if (ret)
		return ret;

	ret = s5k3t2_write_settings(s5k3t2);
	if (ret)
		goto error;

	ret = __v4l2_ctrl_handler_setup(s5k3t2->sd.ctrl_handler);

	cci_write(s5k3t2->regmap, S5K3T2_REG_CTRL_MODE,
		  S5K3T2_MODE_STREAMING, &ret);
	if (ret)
		goto error;

	return 0;

error:
	dev_err(s5k3t2->dev, "failed to start streaming: %d\n", ret);
	pm_runtime_put_autosuspend(s5k3t2->dev);

	return ret;
}

static int s5k3t2_disable_streams(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *state, u32 pad,
				  u64 streams_mask)
{
	struct s5k3t2 *s5k3t2 = to_s5k3t2(sd);
	int ret;

	ret = cci_write(s5k3t2->regmap, S5K3T2_REG_CTRL_MODE, 0x0, NULL);
	if (ret)
		dev_err(s5k3t2->dev, "failed to stop streaming: %d\n", ret);

	pm_runtime_put_autosuspend(s5k3t2->dev);

	return ret;
}

static u32 s5k3t2_get_format_code(struct s5k3t2 *s5k3t2)
{
	unsigned int i;

	i = (s5k3t2->vflip->val ? 2 : 0) | (s5k3t2->hflip->val ? 1 : 0);

	return s5k3t2_mbus_formats[i];
}

static void s5k3t2_update_pad_format(struct s5k3t2 *s5k3t2,
				     const struct s5k3t2_mode *mode,
				     struct v4l2_mbus_framefmt *fmt)
{
	fmt->code = s5k3t2_get_format_code(s5k3t2);
	fmt->width = mode->width;
	fmt->height = mode->height;
	fmt->field = V4L2_FIELD_NONE;
	fmt->colorspace = V4L2_COLORSPACE_RAW;
	fmt->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	fmt->quantization = V4L2_QUANTIZATION_FULL_RANGE;
	fmt->xfer_func = V4L2_XFER_FUNC_NONE;
}

static int s5k3t2_set_pad_format(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *state,
				 struct v4l2_subdev_format *fmt)
{
	struct s5k3t2 *s5k3t2 = to_s5k3t2(sd);
	s64 hblank, vblank, exposure_max;
	const struct s5k3t2_mode *mode;

	mode = v4l2_find_nearest_size(s5k3t2_supported_modes,
				      ARRAY_SIZE(s5k3t2_supported_modes),
				      width, height,
				      fmt->format.width, fmt->format.height);

	s5k3t2_update_pad_format(s5k3t2, mode, &fmt->format);

	/* Format code could be updated with respect to flip controls */
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY || s5k3t2->mode == mode)
		goto set_format;

	__v4l2_ctrl_s_ctrl(s5k3t2->link_freq, mode->link_freq_index);

	hblank = mode->hts - mode->width;
	__v4l2_ctrl_modify_range(s5k3t2->hblank, hblank, hblank, 1, hblank);

	vblank = mode->vts - mode->height;
	__v4l2_ctrl_modify_range(s5k3t2->vblank, vblank,
				 S5K3T2_VTS_MAX - mode->height, 1, vblank);
	__v4l2_ctrl_s_ctrl(s5k3t2->vblank, vblank);

	exposure_max = mode->vts - S5K3T2_EXPOSURE_MARGIN;
	__v4l2_ctrl_modify_range(s5k3t2->exposure, S5K3T2_EXPOSURE_MIN,
				 exposure_max, S5K3T2_EXPOSURE_STEP,
				 mode->exposure);
	__v4l2_ctrl_s_ctrl(s5k3t2->exposure, mode->exposure);

	if (s5k3t2->sd.ctrl_handler->error)
		return s5k3t2->sd.ctrl_handler->error;

	s5k3t2->mode = mode;

set_format:
	*v4l2_subdev_state_get_format(state, 0) = fmt->format;
	*v4l2_subdev_state_get_crop(state, 0) = mode->crop;

	return 0;
}

static int s5k3t2_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct s5k3t2 *s5k3t2 = to_s5k3t2(sd);

	/* Media bus code index is constant, but code formats are not */
	if (code->index > 0)
		return -EINVAL;

	code->code = s5k3t2_get_format_code(s5k3t2);

	return 0;
}

static int s5k3t2_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	struct s5k3t2 *s5k3t2 = to_s5k3t2(sd);

	if (fse->index >= ARRAY_SIZE(s5k3t2_supported_modes))
		return -EINVAL;

	if (fse->code != s5k3t2_get_format_code(s5k3t2))
		return -EINVAL;

	fse->min_width = s5k3t2_supported_modes[fse->index].width;
	fse->max_width = fse->min_width;
	fse->min_height = s5k3t2_supported_modes[fse->index].height;
	fse->max_height = fse->min_height;

	return 0;
}

static int s5k3t2_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_selection *sel)
{
	switch (sel->target) {
	case V4L2_SEL_TGT_CROP:
		sel->r = *v4l2_subdev_state_get_crop(sd_state, 0);
		return 0;
	case V4L2_SEL_TGT_NATIVE_SIZE:
	case V4L2_SEL_TGT_CROP_BOUNDS:
	case V4L2_SEL_TGT_CROP_DEFAULT:
		sel->r.left = 0;
		sel->r.top = 0;
		sel->r.width = S5K3T2_NATIVE_WIDTH;
		sel->r.height = S5K3T2_NATIVE_HEIGHT;
		return 0;
	default:
		return -EINVAL;
	}
}

static int s5k3t2_init_state(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *state)
{
	struct s5k3t2 *s5k3t2 = to_s5k3t2(sd);
	struct v4l2_subdev_format fmt = {
		.which = V4L2_SUBDEV_FORMAT_TRY,
		.pad = 0,
		.format = {
			/* Media bus code depends on current flip controls */
			.width = s5k3t2->mode->width,
			.height = s5k3t2->mode->height,
		},
	};

	return s5k3t2_set_pad_format(sd, state, &fmt);
}

static const struct v4l2_subdev_video_ops s5k3t2_video_ops = {
	.s_stream = v4l2_subdev_s_stream_helper,
};

static const struct v4l2_subdev_pad_ops s5k3t2_pad_ops = {
	.set_fmt = s5k3t2_set_pad_format,
	.get_fmt = v4l2_subdev_get_fmt,
	.get_selection = s5k3t2_get_selection,
	.enum_mbus_code = s5k3t2_enum_mbus_code,
	.enum_frame_size = s5k3t2_enum_frame_size,
	.enable_streams = s5k3t2_enable_streams,
	.disable_streams = s5k3t2_disable_streams,
};

static const struct v4l2_subdev_ops s5k3t2_subdev_ops = {
	.video = &s5k3t2_video_ops,
	.pad = &s5k3t2_pad_ops,
};

static const struct v4l2_subdev_internal_ops s5k3t2_internal_ops = {
	.init_state = s5k3t2_init_state,
};

static const struct media_entity_operations s5k3t2_subdev_entity_ops = {
	.link_validate = v4l2_subdev_link_validate,
};

static int s5k3t2_identify_sensor(struct s5k3t2 *s5k3t2)
{
	u64 val;
	int ret;

	ret = cci_read(s5k3t2->regmap, S5K3T2_REG_CHIP_ID, &val, NULL);
	if (ret)
		return dev_err_probe(s5k3t2->dev, ret,
				     "failed to read chip id\n");

	if (val != S5K3T2_CHIP_ID)
		return dev_err_probe(s5k3t2->dev, -ENODEV,
				     "chip id mismatch: %x!=%llx\n",
				     S5K3T2_CHIP_ID, val);

	return 0;
}

static int s5k3t2_check_hwcfg(struct s5k3t2 *s5k3t2)
{
	struct fwnode_handle *fwnode = dev_fwnode(s5k3t2->dev), *ep;
	struct v4l2_fwnode_endpoint bus_cfg = {
		.bus_type = V4L2_MBUS_CSI2_DPHY,
	};
	unsigned long freq_bitmap;
	int ret;

	if (!fwnode)
		return -ENODEV;

	ep = fwnode_graph_get_next_endpoint(fwnode, NULL);
	if (!ep)
		return -EINVAL;

	ret = v4l2_fwnode_endpoint_alloc_parse(ep, &bus_cfg);
	fwnode_handle_put(ep);
	if (ret)
		return ret;

	if (bus_cfg.bus.mipi_csi2.num_data_lanes != S5K3T2_DATA_LANES) {
		dev_err(s5k3t2->dev, "Invalid number of data lanes: %u\n",
			bus_cfg.bus.mipi_csi2.num_data_lanes);
		ret = -EINVAL;
		goto endpoint_free;
	}

	ret = v4l2_link_freq_to_bitmap(s5k3t2->dev, bus_cfg.link_frequencies,
				       bus_cfg.nr_of_link_frequencies,
				       s5k3t2_link_freq_menu,
				       ARRAY_SIZE(s5k3t2_link_freq_menu),
				       &freq_bitmap);

endpoint_free:
	v4l2_fwnode_endpoint_free(&bus_cfg);

	return ret;
}

static int s5k3t2_power_on(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct s5k3t2 *s5k3t2 = to_s5k3t2(sd);
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(s5k3t2->supplies),
				    s5k3t2->supplies);
	if (ret)
		return ret;

	usleep_range(USEC_PER_MSEC, 2 * USEC_PER_MSEC);

	ret = clk_prepare_enable(s5k3t2->mclk);
	if (ret)
		goto disable_regulators;

	gpiod_set_value_cansleep(s5k3t2->reset_gpio, 0);
	usleep_range(10 * USEC_PER_MSEC, 15 * USEC_PER_MSEC);

	return 0;

disable_regulators:
	regulator_bulk_disable(ARRAY_SIZE(s5k3t2->supplies), s5k3t2->supplies);

	return ret;
}

static int s5k3t2_power_off(struct device *dev)
{
	struct v4l2_subdev *sd = dev_get_drvdata(dev);
	struct s5k3t2 *s5k3t2 = to_s5k3t2(sd);

	gpiod_set_value_cansleep(s5k3t2->reset_gpio, 1);
	clk_disable_unprepare(s5k3t2->mclk);
	regulator_bulk_disable(ARRAY_SIZE(s5k3t2->supplies), s5k3t2->supplies);

	return 0;
}

static int s5k3t2_get_resources(struct s5k3t2 *s5k3t2)
{
	unsigned long freq;
	unsigned int i;
	int ret;

	s5k3t2->mclk = devm_v4l2_sensor_clk_get(s5k3t2->dev, NULL);
	if (IS_ERR(s5k3t2->mclk))
		return dev_err_probe(s5k3t2->dev, PTR_ERR(s5k3t2->mclk),
				     "failed to get MCLK clock\n");

	freq = clk_get_rate(s5k3t2->mclk);
	if (freq != S5K3T2_MCLK_FREQ)
		return dev_err_probe(s5k3t2->dev, -EINVAL,
				     "MCLK clock frequency %lu is not supported\n",
				     freq);

	s5k3t2->reset_gpio = devm_gpiod_get_optional(s5k3t2->dev, "reset",
						     GPIOD_OUT_HIGH);
	if (IS_ERR(s5k3t2->reset_gpio))
		return dev_err_probe(s5k3t2->dev, PTR_ERR(s5k3t2->reset_gpio),
				     "cannot get reset GPIO\n");

	for (i = 0; i < ARRAY_SIZE(s5k3t2->supplies); i++)
		s5k3t2->supplies[i].supply = s5k3t2_supply_names[i];

	ret = devm_regulator_bulk_get(s5k3t2->dev, ARRAY_SIZE(s5k3t2->supplies),
				      s5k3t2->supplies);
	if (ret)
		return dev_err_probe(s5k3t2->dev, ret,
				     "failed to get regulators\n");

	return 0;
}

static int s5k3t2_probe(struct i2c_client *client)
{
	struct s5k3t2 *s5k3t2;
	int ret;

	s5k3t2 = devm_kzalloc(&client->dev, sizeof(*s5k3t2), GFP_KERNEL);
	if (!s5k3t2)
		return -ENOMEM;

	s5k3t2->dev = &client->dev;
	v4l2_i2c_subdev_init(&s5k3t2->sd, client, &s5k3t2_subdev_ops);

	s5k3t2->regmap = devm_cci_regmap_init_i2c(client, 16);
	if (IS_ERR(s5k3t2->regmap))
		return dev_err_probe(s5k3t2->dev, PTR_ERR(s5k3t2->regmap),
				     "failed to init CCI\n");

	ret = s5k3t2_check_hwcfg(s5k3t2);
	if (ret)
		return dev_err_probe(s5k3t2->dev, ret,
				     "failed to check HW configuration\n");

	ret = s5k3t2_get_resources(s5k3t2);
	if (ret)
		return ret;

	/* The sensor must be powered on to read the CHIP_ID register */
	ret = s5k3t2_power_on(s5k3t2->dev);
	if (ret)
		return ret;

	ret = s5k3t2_identify_sensor(s5k3t2);
	if (ret)
		goto power_off;

	s5k3t2->mode = &s5k3t2_supported_modes[0];
	ret = s5k3t2_init_controls(s5k3t2);
	if (ret) {
		dev_err_probe(s5k3t2->dev, ret, "failed to init controls\n");
		goto power_off;
	}

	s5k3t2->sd.state_lock = s5k3t2->ctrl_handler.lock;
	s5k3t2->sd.internal_ops = &s5k3t2_internal_ops;
	s5k3t2->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	s5k3t2->sd.entity.ops = &s5k3t2_subdev_entity_ops;
	s5k3t2->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	s5k3t2->pad.flags = MEDIA_PAD_FL_SOURCE;

	ret = media_entity_pads_init(&s5k3t2->sd.entity, 1, &s5k3t2->pad);
	if (ret) {
		dev_err_probe(s5k3t2->dev, ret,
			      "failed to init media entity pads\n");
		goto v4l2_ctrl_handler_free;
	}

	ret = v4l2_subdev_init_finalize(&s5k3t2->sd);
	if (ret < 0) {
		dev_err_probe(s5k3t2->dev, ret,
			      "failed to finalize the subdev\n");
		goto media_entity_cleanup;
	}

	pm_runtime_set_active(s5k3t2->dev);
	pm_runtime_enable(s5k3t2->dev);

	ret = v4l2_async_register_subdev_sensor(&s5k3t2->sd);
	if (ret < 0) {
		dev_err_probe(s5k3t2->dev, ret,
			      "failed to register V4L2 subdev\n");
		goto subdev_cleanup;
	}

	pm_runtime_set_autosuspend_delay(s5k3t2->dev, 1000);
	pm_runtime_use_autosuspend(s5k3t2->dev);
	pm_runtime_idle(s5k3t2->dev);

	return 0;

subdev_cleanup:
	v4l2_subdev_cleanup(&s5k3t2->sd);
	pm_runtime_disable(s5k3t2->dev);
	pm_runtime_set_suspended(s5k3t2->dev);

media_entity_cleanup:
	media_entity_cleanup(&s5k3t2->sd.entity);

v4l2_ctrl_handler_free:
	v4l2_ctrl_handler_free(s5k3t2->sd.ctrl_handler);

power_off:
	s5k3t2_power_off(s5k3t2->dev);

	return ret;
}

static void s5k3t2_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5k3t2 *s5k3t2 = to_s5k3t2(sd);

	v4l2_async_unregister_subdev(sd);
	v4l2_subdev_cleanup(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(sd->ctrl_handler);
	pm_runtime_disable(s5k3t2->dev);

	if (!pm_runtime_status_suspended(s5k3t2->dev)) {
		s5k3t2_power_off(s5k3t2->dev);
		pm_runtime_set_suspended(s5k3t2->dev);
	}
}

static const struct dev_pm_ops s5k3t2_pm_ops = {
	SET_RUNTIME_PM_OPS(s5k3t2_power_off, s5k3t2_power_on, NULL)
};

static const struct of_device_id s5k3t2_of_match[] = {
	{ .compatible = "samsung,s5k3t2" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, s5k3t2_of_match);

static struct i2c_driver s5k3t2_i2c_driver = {
	.driver = {
		.name = "s5k3t2",
		.pm = &s5k3t2_pm_ops,
		.of_match_table = s5k3t2_of_match,
	},
	.probe = s5k3t2_probe,
	.remove = s5k3t2_remove,
};

module_i2c_driver(s5k3t2_i2c_driver);

MODULE_AUTHOR("Mateus Seiboth <mateusseiboth@gmail.com>");
MODULE_DESCRIPTION("Samsung S5K3T2 image sensor driver");
MODULE_LICENSE("GPL");
