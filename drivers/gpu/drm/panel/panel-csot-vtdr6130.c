// SPDX-License-Identifier: GPL-2.0-only
/*
 * Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 * Copyright (c) 2025, Jens Reidel <adrian@travitia.xyz>
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/display/drm_dsc.h>
#include <drm/display/drm_dsc_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>

struct csot_vtdr6130 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct drm_dsc_config dsc;
	struct gpio_desc *reset_gpio;
	struct regulator_bulk_data *supplies;
};

static const struct regulator_bulk_data csot_vtdr6130_supplies[] = {
	{ .supply = "vdd" }, /* 3p0 */
	{ .supply = "vddio" }, /* 1p8 */
	{ .supply = "dvdd" }, /* 1p2 */
};

static inline struct csot_vtdr6130 *to_csot_vtdr6130(struct drm_panel *panel)
{
	return container_of(panel, struct csot_vtdr6130, panel);
}

static void csot_vtdr6130_reset(struct csot_vtdr6130 *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(11000, 12000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(1000, 2000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(11000, 12000);
}

static int csot_vtdr6130_on(struct csot_vtdr6130 *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = dsi };

	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0x55, 0xaa, 0x52, 0x08,
				     0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xba, 0x01, 0xe6, 0x00, 0x10,
				     0x00, 0x30, 0x00, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb2, 0x58);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x6f, 0x02);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb2, 0x0c, 0x0c);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xbe, 0x0e, 0x0b, 0x14, 0x13);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x6f, 0x05);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xbe, 0x8a);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc0, 0x66);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x6f, 0x08);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb5, 0x32);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0x55, 0xaa, 0x52, 0x08,
				     0x07);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc0, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc1, 0x30, 0x0f, 0x04, 0xc9,
				     0x0f, 0x81, 0xee, 0xc6, 0x3f, 0xfb, 0xb3,
				     0x6a, 0x3f, 0xf6, 0xd1, 0x42, 0x80, 0x00,
				     0xf7, 0x33, 0xb1, 0x00, 0x18, 0x00, 0x00,
				     0x8b, 0x23, 0x33, 0xc0, 0x0f, 0xb9, 0x0f,
				     0xdd, 0x8d, 0x00, 0x00, 0x00, 0x0d, 0x08,
				     0x00, 0x17, 0x23, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc2, 0x38, 0x0f, 0x0b, 0x64,
				     0x02, 0x11, 0xf6, 0x4c, 0x3f, 0xfa, 0xe2,
				     0x14, 0xff, 0xfe, 0x41, 0xa8, 0x00, 0x00,
				     0x5e, 0x26, 0x90, 0x00, 0x00, 0x24, 0x00,
				     0x17, 0x90, 0x33, 0xc0, 0x09, 0xb4, 0x0f,
				     0x94, 0xe9, 0x00, 0x00, 0x90, 0x0d, 0x3c,
				     0x90, 0x17, 0x57, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc3, 0x3c, 0x00, 0x04, 0xc9,
				     0x0f, 0x81, 0x11, 0x3a, 0x3f, 0xf9, 0x58,
				     0x7c, 0x00, 0x04, 0xf1, 0x78, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x90, 0x18, 0x3c, 0x90,
				     0x8b, 0x5f, 0x33, 0x60, 0x00, 0x00, 0x0c,
				     0xdd, 0x73, 0x00, 0x00, 0x04, 0x20, 0x08,
				     0x04, 0x2a, 0x23, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc4, 0x3c, 0x00, 0x0b, 0x64,
				     0x02, 0x11, 0x09, 0xb4, 0x3f, 0xf6, 0xca,
				     0x24, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x80, 0x00, 0xcf, 0x90,
				     0x17, 0x3b, 0x33, 0xc0, 0x00, 0x00, 0x0c,
				     0x94, 0x17, 0x00, 0x00, 0x94, 0x20, 0x3c,
				     0x94, 0x2a, 0x57, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc5, 0x26, 0x00, 0x04, 0xc9,
				     0x0f, 0x81, 0x11, 0x3a, 0x00, 0x00, 0x00,
				     0x00, 0x3f, 0xef, 0x14, 0x34, 0x80, 0x00,
				     0x00, 0x00, 0x00, 0x03, 0xac, 0x00, 0x04,
				     0x1f, 0x23, 0x33, 0xc0, 0x00, 0x00, 0x03,
				     0x23, 0x8d, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc6, 0x2e, 0x00, 0x0b, 0x64,
				     0x02, 0x11, 0x09, 0xb4, 0x00, 0x03, 0x11,
				     0xf4, 0xff, 0xfd, 0x62, 0x7c, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x04, 0x20, 0x24, 0x04,
				     0x37, 0x90, 0x33, 0xc0, 0x00, 0x00, 0x03,
				     0x6c, 0xe9, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc7, 0x2a, 0x0f, 0x04, 0xc9,
				     0x0f, 0x81, 0xee, 0xc6, 0x00, 0x02, 0x5a,
				     0xee, 0x00, 0x0c, 0xae, 0x86, 0x7f, 0xfd,
				     0xf9, 0xf3, 0x65, 0x93, 0xac, 0x3c, 0x94,
				     0x1f, 0x5f, 0x33, 0x6f, 0xf0, 0x47, 0x00,
				     0x23, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc8, 0x2a, 0x0f, 0x0b, 0x64,
				     0x02, 0x11, 0xf6, 0x4c, 0x00, 0x07, 0x29,
				     0xe4, 0xc0, 0x00, 0xdf, 0x2c, 0x7f, 0xff,
				     0x43, 0xb2, 0xe0, 0x84, 0x20, 0xcf, 0x94,
				     0x37, 0x3b, 0x33, 0xcf, 0xf6, 0x4c, 0x00,
				     0x6c, 0x17, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc9, 0x27, 0x00, 0x03, 0xc1,
				     0x04, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x3f, 0xfe, 0xf8, 0x42, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x63, 0x24, 0x00,
				     0x84, 0x43, 0x33, 0x90, 0x00, 0x00, 0x03,
				     0x1f, 0xdf, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xca, 0x21, 0x00, 0x03, 0xc1,
				     0x04, 0x00, 0x00, 0x00, 0x3f, 0xff, 0x0f,
				     0xc0, 0x3f, 0xff, 0x08, 0x00, 0x00, 0x00,
				     0x0f, 0x04, 0x00, 0x00, 0x42, 0x24, 0x00,
				     0x62, 0x43, 0x33, 0x90, 0x03, 0xe0, 0x0f,
				     0xe1, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcb, 0x2d, 0x00, 0x04, 0x00,
				     0x04, 0x00, 0x00, 0x00, 0x3f, 0xff, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x42, 0x44, 0x00,
				     0x62, 0x64, 0x33, 0x60, 0x00, 0x00, 0x0c,
				     0xe0, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcc, 0x2b, 0x00, 0x04, 0x00,
				     0x04, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
				     0xee, 0xfc, 0x00, 0x00, 0x63, 0x44, 0x00,
				     0x84, 0x64, 0x33, 0x6f, 0xfb, 0xe0, 0x00,
				     0x20, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb4, 0xc0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x6f, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb4, 0x00, 0x80, 0x80);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0x55, 0xaa, 0x52, 0x08,
				     0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x6f, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xd2, 0x00, 0x00, 0x11);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x6f, 0x06);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xd2, 0x05);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x6f, 0x0f);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xd2, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x6f, 0x09);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xd2, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xce, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xff, 0xaa, 0x55, 0xa5, 0x80);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x6f, 0x61);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf3, 0x80);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0x55, 0xaa, 0x52, 0x08,
				     0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc0, 0x46);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xbe, 0x0e, 0x0b);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x6f, 0x05);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xbe, 0x88);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x6f, 0x08);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb5, 0x32);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x6f, 0x0b);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb5, 0x33, 0x23, 0x2b);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0x55, 0xaa, 0x52, 0x08,
				     0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xd1, 0x07, 0x00, 0x04);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x3b, 0x00, 0x10, 0x00, 0x30);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xd9, 0xc8);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x90, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x91, 0xab, 0x28, 0x00, 0x0c,
				     0xc2, 0x00, 0x03, 0x1c, 0x01, 0x7e, 0x00,
				     0x0f, 0x08, 0xbb, 0x04, 0x3d, 0x10, 0xf0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x03, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x51, 0x00, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_WRITE_CONTROL_DISPLAY,
				     0x20);
	mipi_dsi_dcs_set_tear_on_multi(&dsi_ctx, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	mipi_dsi_dcs_set_column_address_multi(&dsi_ctx, 0x0000, 0x0437);
	mipi_dsi_dcs_set_page_address_multi(&dsi_ctx, 0x0000, 0x095f);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x2f, 0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xff, 0xaa, 0x55, 0xa5, 0x81);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x6f, 0x0f);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xfd, 0x01, 0x5a);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x6f, 0x04);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xfd, 0x5f);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x6f, 0x1a);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xfd, 0x5f);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_WRITE_MEMORY_START);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0x55, 0xaa, 0x52, 0x08,
				     0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xca, 0x12, 0x00, 0x92, 0x02);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x6f, 0x02);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xec, 0x80, 0x10);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0x55, 0xaa, 0x52, 0x08,
				     0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcd, 0x05, 0x31);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x6f, 0x10);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xd8, 0x0c);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0x55, 0xaa, 0x52, 0x08,
				     0x05);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb3, 0x86, 0x80);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb5, 0x85, 0x81);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb7, 0x85, 0x00, 0x00, 0x81);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb8, 0x05, 0x00, 0x00, 0x81);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xec, 0x0d, 0x11);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x6f, 0x02);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xec, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0x55, 0xaa, 0x52, 0x08,
				     0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x6f, 0x08);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb5, 0x32);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x6f, 0x0b);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb5, 0x33, 0x23, 0x2b);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0x55, 0xaa, 0x52, 0x08,
				     0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xce, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0x55, 0xaa, 0x52, 0x08,
				     0x01);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc3, 0x94, 0x01, 0x97, 0xd0,
				     0x22, 0x02, 0x00);
	mipi_dsi_dcs_exit_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 50);
	mipi_dsi_dcs_set_display_on_multi(&dsi_ctx);
	mipi_dsi_usleep_range(&dsi_ctx, 16000, 17000);

	/*
	 * The init sequence above sets the brightness to 0. Restore the current
	 * backlight level, otherwise the panel stays dark until userspace
	 * touches the backlight (no boot console, black screen until the UI).
	 */
	if (ctx->panel.backlight) {
		u16 level = ctx->panel.backlight->props.brightness;
		u8 cmd[] = { MIPI_DCS_SET_DISPLAY_BRIGHTNESS, level >> 8, level & 0xff };

		mipi_dsi_dcs_write_buffer_multi(&dsi_ctx, cmd, sizeof(cmd));
	}

	return dsi_ctx.accum_err;
}

static int csot_vtdr6130_disable(struct drm_panel *panel)
{
	struct csot_vtdr6130 *ctx = to_csot_vtdr6130(panel);
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = dsi };

	mipi_dsi_dcs_set_display_off_multi(&dsi_ctx);
	mipi_dsi_usleep_range(&dsi_ctx, 20000, 21000);
	mipi_dsi_dcs_enter_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_usleep_range(&dsi_ctx, 80000, 81000);

	return dsi_ctx.accum_err;
}

static int csot_vtdr6130_prepare(struct drm_panel *panel)
{
	struct csot_vtdr6130 *ctx = to_csot_vtdr6130(panel);
	struct drm_dsc_picture_parameter_set pps;
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(csot_vtdr6130_supplies),
				    ctx->supplies);
	if (ret < 0)
		return ret;

	csot_vtdr6130_reset(ctx);

	ret = csot_vtdr6130_on(ctx);
	if (ret < 0) {
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		goto err;
	}

	drm_dsc_pps_payload_pack(&pps, &ctx->dsc);
	mipi_dsi_picture_parameter_set_multi(&dsi_ctx, &pps);
	mipi_dsi_compression_mode_ext_multi(&dsi_ctx, true,
					    MIPI_DSI_COMPRESSION_DSC, 0);

	mipi_dsi_msleep(&dsi_ctx, 28);

	if (dsi_ctx.accum_err < 0) {
		ret = dsi_ctx.accum_err;
		goto err;
	}

	return dsi_ctx.accum_err;
err:
	regulator_bulk_disable(ARRAY_SIZE(csot_vtdr6130_supplies),
			       ctx->supplies);
	return ret;
}

static int csot_vtdr6130_unprepare(struct drm_panel *panel)
{
	struct csot_vtdr6130 *ctx = to_csot_vtdr6130(panel);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	regulator_bulk_disable(ARRAY_SIZE(csot_vtdr6130_supplies),
			       ctx->supplies);

	return 0;
}

static const struct drm_display_mode csot_vtdr6130_modes[] = {
	{
		/* 90Hz mode */
		.clock =
			(1080 + 16 + 8 + 8) * (2400 + 1212 + 4 + 8) * 90 / 1000,
		.hdisplay = 1080,
		.hsync_start = 1080 + 16,
		.hsync_end = 1080 + 16 + 8,
		.htotal = 1080 + 16 + 8 + 8,
		.vdisplay = 2400,
		.vsync_start = 2400 + 1212,
		.vsync_end = 2400 + 1212 + 4,
		.vtotal = 2400 + 1212 + 4 + 8,
		.width_mm = 68,
		.height_mm = 152,
		.type = DRM_MODE_TYPE_DRIVER,
	},
	{
		/* 60Hz mode */
		.clock =
			(1080 + 16 + 8 + 8) * (2400 + 1212 + 4 + 8) * 60 / 1000,
		.hdisplay = 1080,
		.hsync_start = 1080 + 16,
		.hsync_end = 1080 + 16 + 8,
		.htotal = 1080 + 16 + 8 + 8,
		.vdisplay = 2400,
		.vsync_start = 2400 + 1212,
		.vsync_end = 2400 + 1212 + 4,
		.vtotal = 2400 + 1212 + 4 + 8,
		.width_mm = 68,
		.height_mm = 152,
		.type = DRM_MODE_TYPE_DRIVER,
	},
};

static int csot_vtdr6130_get_modes(struct drm_panel *panel,
				   struct drm_connector *connector)
{
	int count = 0;

	for (int i = 0; i < ARRAY_SIZE(csot_vtdr6130_modes); i++) {
		count += drm_connector_helper_get_modes_fixed(
			connector, &csot_vtdr6130_modes[i]);
	}

	return count;
}

static const struct drm_panel_funcs csot_vtdr6130_panel_funcs = {
	.prepare = csot_vtdr6130_prepare,
	.unprepare = csot_vtdr6130_unprepare,
	.disable = csot_vtdr6130_disable,
	.get_modes = csot_vtdr6130_get_modes,
};

static int csot_vtdr6130_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	u16 brightness = backlight_get_brightness(bl);
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_brightness_large(dsi, brightness);
	if (ret < 0)
		return ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	return 0;
}

static int csot_vtdr6130_bl_get_brightness(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	u16 brightness;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_get_display_brightness_large(dsi, &brightness);
	if (ret < 0)
		return ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	return brightness;
}

static const struct backlight_ops csot_vtdr6130_bl_ops = {
	.update_status = csot_vtdr6130_bl_update_status,
	.get_brightness = csot_vtdr6130_bl_get_brightness,
};

static struct backlight_device *
csot_vtdr6130_create_backlight(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct backlight_properties props = {
		.type = BACKLIGHT_RAW,
		.brightness = 2047,
		.max_brightness = 4095,
	};

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					      &csot_vtdr6130_bl_ops, &props);
}

static int csot_vtdr6130_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct csot_vtdr6130 *ctx;
	int ret;

	ctx = devm_drm_panel_alloc(dev, struct csot_vtdr6130, panel,
				   &csot_vtdr6130_panel_funcs,
				   DRM_MODE_CONNECTOR_DSI);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);

	ret = devm_regulator_bulk_get_const(&dsi->dev,
					    ARRAY_SIZE(csot_vtdr6130_supplies),
					    csot_vtdr6130_supplies,
					    &ctx->supplies);
	if (ret < 0)
		return dev_err_probe(dev, ret, "Failed to get regulators\n");

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_LPM;

	ctx->panel.prepare_prev_first = true;

	ctx->panel.backlight = csot_vtdr6130_create_backlight(dsi);
	if (IS_ERR(ctx->panel.backlight))
		return dev_err_probe(dev, PTR_ERR(ctx->panel.backlight),
				     "Failed to create backlight\n");

	drm_panel_add(&ctx->panel);

	dsi->dsc = &ctx->dsc;
	ctx->dsc.dsc_version_major = 1;
	ctx->dsc.dsc_version_minor = 1;
	ctx->dsc.slice_height = 12;
	ctx->dsc.slice_width = 1080;
	ctx->dsc.slice_count = 1080 / ctx->dsc.slice_width;
	ctx->dsc.bits_per_component = 8;
	ctx->dsc.bits_per_pixel = 8 << 4;
	ctx->dsc.block_pred_enable = true;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		return dev_err_probe(dev, ret,
				     "Failed to attach to DSI host\n");
	}

	return 0;
}

static void csot_vtdr6130_remove(struct mipi_dsi_device *dsi)
{
	struct csot_vtdr6130 *ctx = mipi_dsi_get_drvdata(dsi);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id csot_vtdr6130_of_match[] = {
	{ .compatible = "csot,vtdr6130" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, csot_vtdr6130_of_match);

static struct mipi_dsi_driver csot_vtdr6130_driver = {
	.probe = csot_vtdr6130_probe,
	.remove = csot_vtdr6130_remove,
	.driver = {
		.name = "panel-csot-vtdr6130",
		.of_match_table = csot_vtdr6130_of_match,
	},
};
module_mipi_dsi_driver(csot_vtdr6130_driver);

MODULE_AUTHOR("Jens Reidel <adrian@travitia.xyz>");
MODULE_DESCRIPTION("Panel driver for the CSOT VTDR6130 AMOLED DSI panel");
MODULE_LICENSE("GPL");
