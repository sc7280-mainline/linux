// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2026 Luka Panio
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved.

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/mod_devicetable.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>

static const char * const regulator_names[] = {
	"vddio",
	"vddpos",
	"vddneg",
};

struct focaltech_ft8722 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator_bulk_data supplies[ARRAY_SIZE(regulator_names)];
	struct gpio_desc *reset_gpio;
};

static inline
struct focaltech_ft8722 *to_focaltech_ft8722(struct drm_panel *panel)
{
	return container_of(panel, struct focaltech_ft8722, panel);
}

static void focaltech_ft8722_reset(struct focaltech_ft8722 *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(10000, 11000);
}

static int focaltech_ft8722_on(struct focaltech_ft8722 *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb6, 0x07, 0x20);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xff, 0x87, 0x22, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x80);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xff, 0x87, 0x22);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xa3);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb3, 0x09, 0x68);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x80);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc0,
				     0x00, 0xb0, 0x00, 0x2c, 0x00, 0x14);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x90);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc0,
				     0x00, 0x4b, 0x00, 0x2c, 0x00, 0x14);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xa0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc0,
				     0x00, 0x4b, 0x00, 0x2c, 0x00, 0x14);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xb0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc0,
				     0x00, 0xd4, 0x00, 0x2c, 0x14);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xc1);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc0,
				     0x01, 0x08, 0x00, 0xd1, 0x00, 0xb0, 0x01,
				     0x3a);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x70);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc0,
				     0x00, 0x4b, 0x00, 0x2c, 0x00, 0x14);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xa3);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc1,
				     0x00, 0x54, 0x00, 0x54, 0x00, 0x02);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xb7);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc1, 0x00, 0x54);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x80);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xce,
				     0x01, 0x81, 0xff, 0xff, 0x00, 0x85, 0x00,
				     0x85, 0x00, 0xc8, 0x00, 0xc8, 0x00, 0xc8,
				     0x00, 0xc8);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x90);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xce,
				     0x00, 0xbd, 0x12, 0x75, 0x00, 0xbd, 0x80,
				     0xff, 0xff, 0x00, 0x06, 0x40, 0x0a, 0x0d,
				     0x0d);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xa0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xce, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xb0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xce, 0x22, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xd1);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xce,
				     0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xe1);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xce,
				     0x0a, 0x03, 0x14, 0x03, 0x14, 0x03, 0x14,
				     0x00, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xf1);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xce,
				     0x0e, 0x2a, 0x2a, 0x01, 0x42, 0x01, 0x09,
				     0x01, 0x09);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xb0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcf, 0x00, 0x00, 0xc2, 0xc6);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xb5);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcf, 0x05, 0x05, 0x72, 0x76);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xc0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcf, 0x09, 0x09, 0x63, 0x67);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xc5);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcf, 0x09, 0x09, 0x69, 0x6d);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x60);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcf,
				     0x00, 0x00, 0xc2, 0xc6, 0x05, 0x05, 0x72,
				     0x76);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x70);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcf,
				     0x00, 0x00, 0xc2, 0xc6, 0x05, 0x05, 0x72,
				     0x76);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xd1);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc1,
				     0x0a, 0xec, 0x0f, 0x19, 0x19, 0xd4, 0x0a,
				     0xce, 0x0f, 0x19, 0x19, 0xd4);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xe1);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc1, 0x0f, 0x19);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xe4);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcf,
				     0x09, 0xf8, 0x09, 0xf7, 0x09, 0xf7, 0x09,
				     0xf7, 0x09, 0xf7, 0x09, 0xf7);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x80);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc1, 0x44, 0x44);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x90);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc1, 0x03);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xf5);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcf, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xf6);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcf, 0x78);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xf1);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcf, 0x78);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xf7);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcf, 0x11);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x8f);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc5, 0x20);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x80);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc2,
				     0x82, 0x01, 0x25, 0x25, 0x00, 0x00, 0x00,
				     0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x90);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc2, 0x00, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xa0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc2,
				     0x00, 0x80, 0x00, 0x17, 0x8a, 0x01, 0x00,
				     0x00, 0x17, 0x8a, 0x02, 0x01, 0x00, 0x17,
				     0x8a);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xb0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc2,
				     0x03, 0x02, 0x00, 0x17, 0x8a, 0x80, 0x08,
				     0x03, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xca);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc2,
				     0x84, 0x08, 0x03, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xe0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc2,
				     0x33, 0x33, 0x70, 0x00, 0x70);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xe8);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc2,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x80);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcb,
				     0x00, 0x01, 0x00, 0x03, 0xfd, 0x01, 0x01,
				     0x00, 0x00, 0x00, 0xfd, 0x01, 0x00, 0x03,
				     0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x90);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcb,
				     0x00, 0x00, 0x00, 0x0c, 0xf0, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00,
				     0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xa0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcb,
				     0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x0c,
				     0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xb0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcb, 0x13, 0x54, 0x05, 0x30);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xc0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcb, 0x13, 0x54, 0x05, 0x30);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xd5);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcb,
				     0x01, 0x00, 0x01, 0x01, 0x00, 0x01, 0x01,
				     0x00, 0x01, 0x01, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xe0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcb,
				     0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x01,
				     0x01, 0x00, 0x01, 0x01, 0x00, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x80);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcc,
				     0x2c, 0x12, 0x2c, 0x22, 0x2c, 0x0a, 0x2c,
				     0x2c, 0x09, 0x08, 0x07, 0x06, 0x2c, 0x2c,
				     0x2c, 0x2c);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x90);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcc,
				     0x2c, 0x18, 0x16, 0x17, 0x2c, 0x1c, 0x1d,
				     0x1e);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xa0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcc,
				     0x2c, 0x12, 0x2c, 0x23, 0x2c, 0x0e, 0x2c,
				     0x2c, 0x06, 0x07, 0x08, 0x09, 0x2c, 0x2c,
				     0x2c, 0x2c);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xb0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcc,
				     0x2c, 0x18, 0x16, 0x17, 0x2c, 0x1c, 0x1d,
				     0x1e);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x80);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcd,
				     0x2c, 0x2c, 0x2c, 0x02, 0x2c, 0x0a, 0x2c,
				     0x2c, 0x09, 0x08, 0x07, 0x06, 0x2c, 0x2c,
				     0x2c, 0x2c);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x90);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcd,
				     0x2c, 0x18, 0x16, 0x17, 0x2c, 0x1c, 0x1d,
				     0x1e);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xa0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcd,
				     0x2c, 0x2c, 0x2c, 0x02, 0x2c, 0x0e, 0x2c,
				     0x2c, 0x06, 0x07, 0x08, 0x09, 0x2c, 0x2c,
				     0x2c, 0x2c);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xb0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcd,
				     0x2c, 0x18, 0x16, 0x17, 0x2c, 0x1c, 0x1d,
				     0x1e);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x86);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc0,
				     0x00, 0x00, 0x00, 0x01, 0x11, 0x11, 0x11,
				     0x05);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x96);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc0,
				     0x00, 0x00, 0x00, 0x01, 0x11, 0x11, 0x11,
				     0x05);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xa6);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc0,
				     0x00, 0x00, 0x00, 0x01, 0x11, 0x11, 0x11,
				     0x05);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xa3);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xce,
				     0x00, 0x00, 0x00, 0x01, 0x11, 0x05);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xb3);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xce,
				     0x00, 0x00, 0x00, 0x01, 0x11, 0x05);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x76);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc0,
				     0x00, 0x00, 0x00, 0x01, 0x11, 0x11, 0x11,
				     0x05);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x82);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xa7, 0x10, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x8d);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xa7, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x8f);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xa7, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xa0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc3, 0x35, 0x21);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xa4);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc3, 0x01, 0x20);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xaa);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc3, 0x21);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xad);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc3, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xae);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc3, 0x20);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xb3);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc3, 0x21);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xb6);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc3, 0x01, 0x20);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xc3);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc5, 0xff);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xa9);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf5, 0x8e);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x93);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc5, 0x25);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x97);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc5, 0x25);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x9a);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc5, 0x21);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x9c);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc5, 0x21);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xb6);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc5,
				     0x10, 0x10, 0x0e, 0x0e, 0x10, 0x10, 0x0e,
				     0x0e);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x90);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc3, 0x08);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xfa);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc2, 0x23);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xca);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc0, 0x80);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x82);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf5, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x93);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf5, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x9b);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf5, 0x49);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x9d);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf5, 0x49);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xbe);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc5, 0xf0, 0xf0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xdc);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc3, 0x37);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x8a);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf5, 0xc7);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xb1);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf5, 0x1f);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xb7);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf5, 0x1f);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x99);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcf, 0x50);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x9c);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf5, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x9e);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf5, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xb0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc5,
				     0xc0, 0x4a, 0x39, 0xc0, 0x4a, 0x0e);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xc2);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf5, 0x42);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x80);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc5, 0x77);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xe8);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc0, 0x40);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xe1,
				     0x00, 0x02, 0x07, 0x0f, 0x36, 0x1a, 0x22,
				     0x29, 0x34, 0xba, 0x3d, 0x44, 0x4a, 0x4f,
				     0x1b, 0x54, 0x5d, 0x65, 0x6c, 0xd6, 0x73,
				     0x7b, 0x83, 0x8c, 0xd4, 0x96, 0x9c, 0xa3,
				     0xaa, 0x93, 0xb3, 0xbe, 0xcd, 0xd5, 0xf3,
				     0xe0, 0xef, 0xf9, 0xff, 0x84);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x30);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xe1,
				     0x00, 0x02, 0x07, 0x0f, 0x36, 0x1a, 0x22,
				     0x29, 0x34, 0xba, 0x3d, 0x44, 0x4a, 0x4f,
				     0x1b, 0x54, 0x5d, 0x65, 0x6c, 0xd6, 0x73,
				     0x7b, 0x83, 0x8c, 0xd4, 0x96, 0x9c, 0xa3,
				     0xaa, 0x93, 0xb3, 0xbe, 0xcd, 0xd5, 0xf3,
				     0xe0, 0xef, 0xf9, 0xff, 0x84);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x60);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xe1,
				     0x00, 0x02, 0x07, 0x0f, 0x36, 0x1a, 0x22,
				     0x29, 0x34, 0xba, 0x3d, 0x44, 0x4a, 0x4f,
				     0x1b, 0x54, 0x5d, 0x65, 0x6c, 0xd6, 0x73,
				     0x7b, 0x83, 0x8c, 0xd4, 0x96, 0x9c, 0xa3,
				     0xaa, 0x93, 0xb3, 0xbe, 0xcd, 0xd5, 0xf3,
				     0xe0, 0xef, 0xf9, 0xff, 0x84);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x90);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xe1,
				     0x00, 0x02, 0x07, 0x0f, 0x36, 0x1a, 0x22,
				     0x29, 0x34, 0xba, 0x3d, 0x44, 0x4a, 0x4f,
				     0x1b, 0x54, 0x5d, 0x65, 0x6c, 0xd6, 0x73,
				     0x7b, 0x83, 0x8c, 0xd4, 0x96, 0x9c, 0xa3,
				     0xaa, 0x93, 0xb3, 0xbe, 0xcd, 0xd5, 0xf3,
				     0xe0, 0xef, 0xf9, 0xff, 0x84);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xc0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xe1,
				     0x00, 0x02, 0x07, 0x0f, 0x36, 0x1a, 0x22,
				     0x29, 0x34, 0xba, 0x3d, 0x44, 0x4a, 0x4f,
				     0x1b, 0x54, 0x5d, 0x65, 0x6c, 0xd6, 0x73,
				     0x7b, 0x83, 0x8c, 0xd4, 0x96, 0x9c, 0xa3,
				     0xaa, 0x93, 0xb3, 0xbe, 0xcd, 0xd5, 0xf3,
				     0xe0, 0xef, 0xf9, 0xff, 0x84);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xf0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xe1,
				     0x00, 0x02, 0x07, 0x0f, 0x36, 0x1a, 0x22,
				     0x29, 0x34, 0xba, 0x3d, 0x44, 0x4a, 0x4f,
				     0x1b, 0x54);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xe2,
				     0x5d, 0x65, 0x6c, 0xd6, 0x73, 0x7b, 0x83,
				     0x8c, 0xd4, 0x96, 0x9c, 0xa3, 0xaa, 0x93,
				     0xb3, 0xbe, 0xcd, 0xd5, 0xf3, 0xe0, 0xef,
				     0xf9, 0xff, 0x84);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xe0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcf, 0x34);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x85);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xa7, 0x41);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x80);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb3, 0x22);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xb0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb3, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x83);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb0, 0x63);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xa1);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb0, 0x02);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xa9);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb0, 0xaa, 0x0a);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xb0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xca, 0x01, 0x01, 0x0c);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0xb5);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xca, 0x08);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x1c, 0x02);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xff, 0xff, 0xff, 0xff);
	mipi_dsi_dcs_exit_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 120);
	mipi_dsi_dcs_set_display_on_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 50);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb7, 0x59, 0x02);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xff, 0xff, 0xff);

	return dsi_ctx.accum_err;
}

static int focaltech_ft8722_off(struct focaltech_ft8722 *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	mipi_dsi_dcs_set_display_off_multi(&dsi_ctx);
	mipi_dsi_usleep_range(&dsi_ctx, 16000, 17000);
	mipi_dsi_dcs_enter_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 50);

	return dsi_ctx.accum_err;
}

static int focaltech_ft8722_prepare(struct drm_panel *panel)
{
	struct focaltech_ft8722 *ctx = to_focaltech_ft8722(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	if (ret < 0)
		return ret;

	focaltech_ft8722_reset(ctx);

	ret = focaltech_ft8722_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		return ret;
	}

	return 0;
}

static int focaltech_ft8722_unprepare(struct drm_panel *panel)
{
	struct focaltech_ft8722 *ctx = to_focaltech_ft8722(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = focaltech_ft8722_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);

	return 0;
}

static const struct drm_display_mode focaltech_ft8722_mode = {
	.clock = (1080 + 32 + 8 + 32) * (2408 + 30 + 4 + 30) * 120 / 1000,
	.hdisplay = 1080,
	.hsync_start = 1080 + 32,
	.hsync_end = 1080 + 32 + 8,
	.htotal = 1080 + 32 + 8 + 32,
	.vdisplay = 2408,
	.vsync_start = 2408 + 30,
	.vsync_end = 2408 + 30 + 4,
	.vtotal = 2408 + 30 + 4 + 30,
	.width_mm = 68,
	.height_mm = 153,
	.type = DRM_MODE_TYPE_DRIVER,
};

static int focaltech_ft8722_get_modes(struct drm_panel *panel,
					    struct drm_connector *connector)
{
	return drm_connector_helper_get_modes_fixed(connector, &focaltech_ft8722_mode);
}

static const struct drm_panel_funcs focaltech_ft8722_panel_funcs = {
	.prepare = focaltech_ft8722_prepare,
	.unprepare = focaltech_ft8722_unprepare,
	.get_modes = focaltech_ft8722_get_modes,
};

static int focaltech_ft8722_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct focaltech_ft8722 *ctx;
	int ret;
	int i;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(ctx->supplies); i++)
		ctx->supplies[i].supply = regulator_names[i];

	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(ctx->supplies),
				      ctx->supplies);
	if (ret < 0)
		return dev_err_probe(dev, ret, "Failed to get regulators\n");

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 3;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_LPM;

	drm_panel_init(&ctx->panel, dev, &focaltech_ft8722_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);
	ctx->panel.prepare_prev_first = true;

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		return dev_err_probe(dev, ret, "Failed to attach to DSI host\n");
	}

	return 0;
}

static void focaltech_ft8722_remove(struct mipi_dsi_device *dsi)
{
	struct focaltech_ft8722 *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id focaltech_ft8722_of_match[] = {
	{ .compatible = "txd,txdi660jbpv" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, focaltech_ft8722_of_match);

static struct mipi_dsi_driver ft8722_driver = {
	.probe = focaltech_ft8722_probe,
	.remove = focaltech_ft8722_remove,
	.driver = {
		.name = "panel-focaltech-ft8722",
		.of_match_table = focaltech_ft8722_of_match,
	},
};
module_mipi_dsi_driver(ft8722_driver);

MODULE_AUTHOR("Luka Pani <lukapanio@gmail.com>");
MODULE_DESCRIPTION("DRM driver for ft8722 panel");
MODULE_LICENSE("GPL");
