// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2024 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>

#include <video/mipi_display.h>

#include <drm/display/drm_dsc.h>
#include <drm/display/drm_dsc_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct rm692e5_visionox_amoled_120hz {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct drm_dsc_config dsc;
	struct gpio_desc *reset_gpio;
	bool prepared;
};

static inline
struct rm692e5_visionox_amoled_120hz *to_rm692e5_visionox_amoled_120hz(struct drm_panel *panel)
{
	return container_of(panel, struct rm692e5_visionox_amoled_120hz, panel);
}

static void rm692e5_visionox_amoled_120hz_reset(struct rm692e5_visionox_amoled_120hz *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(1000, 2000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(32);
}

static int rm692e5_visionox_amoled_120hz_on(struct rm692e5_visionox_amoled_120hz *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	mipi_dsi_dcs_write_seq(dsi, 0xfe, 0x40);
	mipi_dsi_dcs_write_seq(dsi, 0xbd, 0x00);
	msleep(34);
	mipi_dsi_dcs_write_seq(dsi, 0xfe, 0xd2);
	mipi_dsi_dcs_write_seq(dsi, 0x50, 0x11);

	ret = mipi_dsi_dcs_set_display_brightness(dsi, 0x00ab);
	if (ret < 0) {
		dev_err(dev, "Failed to set display brightness: %d\n", ret);
		return ret;
	}

	mipi_dsi_dcs_write_seq(dsi, 0x52, 0x30);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x09);
	mipi_dsi_dcs_write_seq(dsi, 0x54, 0x60);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_POWER_SAVE, 0x04);
	mipi_dsi_dcs_write_seq(dsi, 0x56, 0x38);
	mipi_dsi_dcs_write_seq(dsi, 0x58, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x59, 0x14);
	mipi_dsi_dcs_write_seq(dsi, 0x5a, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0x5b, 0x1c);
	mipi_dsi_dcs_write_seq(dsi, 0x5c, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0x5d, 0x00);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_CABC_MIN_BRIGHTNESS, 0x20);
	mipi_dsi_dcs_write_seq(dsi, 0x5f, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x60, 0xe8);
	mipi_dsi_dcs_write_seq(dsi, 0x61, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x62, 0x07);
	mipi_dsi_dcs_write_seq(dsi, 0x63, 0x0c);
	mipi_dsi_dcs_write_seq(dsi, 0x64, 0x05);
	mipi_dsi_dcs_write_seq(dsi, 0x65, 0x0e);
	mipi_dsi_dcs_write_seq(dsi, 0x66, 0x05);
	mipi_dsi_dcs_write_seq(dsi, 0x67, 0x16);
	mipi_dsi_dcs_write_seq(dsi, 0x68, 0x18);
	mipi_dsi_dcs_write_seq(dsi, 0x69, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x6a, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0x6b, 0xf0);
	mipi_dsi_dcs_write_seq(dsi, 0x6c, 0x07);
	mipi_dsi_dcs_write_seq(dsi, 0x6d, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0x6e, 0x20);
	mipi_dsi_dcs_write_seq(dsi, 0x6f, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x70, 0x06);
	mipi_dsi_dcs_write_seq(dsi, 0x71, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0x72, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0x73, 0x33);
	mipi_dsi_dcs_write_seq(dsi, 0x74, 0x0e);
	mipi_dsi_dcs_write_seq(dsi, 0x75, 0x1c);
	mipi_dsi_dcs_write_seq(dsi, 0x76, 0x2a);
	mipi_dsi_dcs_write_seq(dsi, 0x77, 0x38);
	mipi_dsi_dcs_write_seq(dsi, 0x78, 0x46);
	mipi_dsi_dcs_write_seq(dsi, 0x79, 0x54);
	mipi_dsi_dcs_write_seq(dsi, 0x7a, 0x62);
	mipi_dsi_dcs_write_seq(dsi, 0x7b, 0x69);
	mipi_dsi_dcs_write_seq(dsi, 0x7c, 0x70);
	mipi_dsi_dcs_write_seq(dsi, 0x7d, 0x77);
	mipi_dsi_dcs_write_seq(dsi, 0x7e, 0x79);
	mipi_dsi_dcs_write_seq(dsi, 0x7f, 0x7b);
	mipi_dsi_dcs_write_seq(dsi, 0x80, 0x7d);
	mipi_dsi_dcs_write_seq(dsi, 0x81, 0x7e);
	mipi_dsi_dcs_write_seq(dsi, 0x82, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x83, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0x84, 0x22);
	mipi_dsi_dcs_write_seq(dsi, 0x85, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x86, 0x2a);
	mipi_dsi_dcs_write_seq(dsi, 0x87, 0x40);
	mipi_dsi_dcs_write_seq(dsi, 0x88, 0x2a);
	mipi_dsi_dcs_write_seq(dsi, 0x89, 0xbe);
	mipi_dsi_dcs_write_seq(dsi, 0x8a, 0x3a);
	mipi_dsi_dcs_write_seq(dsi, 0x8b, 0xfc);
	mipi_dsi_dcs_write_seq(dsi, 0x8c, 0x3a);
	mipi_dsi_dcs_write_seq(dsi, 0x8d, 0xfa);
	mipi_dsi_dcs_write_seq(dsi, 0x8e, 0x3a);
	mipi_dsi_dcs_write_seq(dsi, 0x8f, 0xf8);
	mipi_dsi_dcs_write_seq(dsi, 0x90, 0x3b);
	mipi_dsi_dcs_write_seq(dsi, 0x91, 0x38);
	mipi_dsi_dcs_write_seq(dsi, 0x92, 0x3b);
	mipi_dsi_dcs_write_seq(dsi, 0x93, 0x78);
	mipi_dsi_dcs_write_seq(dsi, 0x94, 0x3b);
	mipi_dsi_dcs_write_seq(dsi, 0x95, 0xb6);
	mipi_dsi_dcs_write_seq(dsi, 0x96, 0x4b);
	mipi_dsi_dcs_write_seq(dsi, 0x97, 0xf6);
	mipi_dsi_dcs_write_seq(dsi, 0x98, 0x4c);
	mipi_dsi_dcs_write_seq(dsi, 0x99, 0x34);
	mipi_dsi_dcs_write_seq(dsi, 0x9a, 0x4c);
	mipi_dsi_dcs_write_seq(dsi, 0x9b, 0x74);
	mipi_dsi_dcs_write_seq(dsi, 0x9c, 0x5c);
	mipi_dsi_dcs_write_seq(dsi, 0x9d, 0x74);
	mipi_dsi_dcs_write_seq(dsi, 0x9e, 0x8c);
	mipi_dsi_dcs_write_seq(dsi, 0x9f, 0xf4);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_READ_PPS_START, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0xa3, 0x1c);
	mipi_dsi_dcs_write_seq(dsi, 0xa4, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xa5, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xa6, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xa7, 0x00);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_READ_PPS_CONTINUE, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xaa, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xa0, 0x80);
	mipi_dsi_dcs_write_seq(dsi, 0xfe, 0xa1);
	mipi_dsi_dcs_write_seq(dsi, 0xcd, 0x6b);
	mipi_dsi_dcs_write_seq(dsi, 0xce, 0xbb);
	mipi_dsi_dcs_write_seq(dsi, 0xfe, 0xd1);
	mipi_dsi_dcs_write_seq(dsi, 0xb4, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0xfe, 0x38);
	mipi_dsi_dcs_write_seq(dsi, 0x17, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0x18, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0xfe, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xfa, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0xc2, 0x08);

	ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	if (ret < 0) {
		dev_err(dev, "Failed to set tear on: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_dcs_set_display_brightness(dsi, 0x000d);
	if (ret < 0) {
		dev_err(dev, "Failed to set display brightness: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	msleep(50);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display on: %d\n", ret);
		return ret;
	}
	usleep_range(1000, 2000);

	return 0;
}

static int rm692e5_visionox_amoled_120hz_off(struct rm692e5_visionox_amoled_120hz *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display off: %d\n", ret);
		return ret;
	}
	usleep_range(1000, 2000);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
		return ret;
	}
	usleep_range(1000, 2000);

	return 0;
}

static int rm692e5_visionox_amoled_120hz_prepare(struct drm_panel *panel)
{
	struct rm692e5_visionox_amoled_120hz *ctx = to_rm692e5_visionox_amoled_120hz(panel);
	struct device *dev = &ctx->dsi->dev;
	struct drm_dsc_picture_parameter_set pps;
	int ret;

	if (ctx->prepared)
		return 0;

	rm692e5_visionox_amoled_120hz_reset(ctx);

	ret = rm692e5_visionox_amoled_120hz_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		return ret;
	}

	drm_dsc_pps_payload_pack(&pps, &ctx->dsc);

	ret = mipi_dsi_picture_parameter_set(ctx->dsi, &pps);
	if (ret < 0) {
		dev_err(panel->dev, "failed to transmit PPS: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_compression_mode(ctx->dsi, true);
	if (ret < 0) {
		dev_err(dev, "failed to enable compression mode: %d\n", ret);
		return ret;
	}

	msleep(28); /* TODO: Is this panel-dependent? */

	ctx->prepared = true;
	return 0;
}

static int rm692e5_visionox_amoled_120hz_unprepare(struct drm_panel *panel)
{
	struct rm692e5_visionox_amoled_120hz *ctx = to_rm692e5_visionox_amoled_120hz(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = rm692e5_visionox_amoled_120hz_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode rm692e5_visionox_amoled_120hz_mode = {
	.clock = (1080 + 26 + 39 + 36) * (2400 + 16 + 21 + 16) * 60 / 1000,
	.hdisplay = 1080,
	.hsync_start = 1080 + 26,
	.hsync_end = 1080 + 26 + 39,
	.htotal = 1080 + 26 + 39 + 36,
	.vdisplay = 2400,
	.vsync_start = 2400 + 16,
	.vsync_end = 2400 + 16 + 21,
	.vtotal = 2400 + 16 + 21 + 16,
	.width_mm = 68,
	.height_mm = 152,
};

static int rm692e5_visionox_amoled_120hz_get_modes(struct drm_panel *panel,
						   struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &rm692e5_visionox_amoled_120hz_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs rm692e5_visionox_amoled_120hz_panel_funcs = {
	.prepare = rm692e5_visionox_amoled_120hz_prepare,
	.unprepare = rm692e5_visionox_amoled_120hz_unprepare,
	.get_modes = rm692e5_visionox_amoled_120hz_get_modes,
};

static int rm692e5_visionox_amoled_120hz_bl_update_status(struct backlight_device *bl)
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

// TODO: Check if /sys/class/backlight/.../actual_brightness actually returns
// correct values. If not, remove this function.
static int rm692e5_visionox_amoled_120hz_bl_get_brightness(struct backlight_device *bl)
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

static const struct backlight_ops rm692e5_visionox_amoled_120hz_bl_ops = {
	.update_status = rm692e5_visionox_amoled_120hz_bl_update_status,
	.get_brightness = rm692e5_visionox_amoled_120hz_bl_get_brightness,
};

static struct backlight_device *
rm692e5_visionox_amoled_120hz_create_backlight(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct backlight_properties props = {
		.type = BACKLIGHT_RAW,
		.brightness = 4095,
		.max_brightness = 4095,
	};

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					      &rm692e5_visionox_amoled_120hz_bl_ops, &props);
}

static int rm692e5_visionox_amoled_120hz_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct rm692e5_visionox_amoled_120hz *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_CLOCK_NON_CONTINUOUS;

	drm_panel_init(&ctx->panel, dev, &rm692e5_visionox_amoled_120hz_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);
	ctx->panel.prepare_prev_first = true;

	ctx->panel.backlight = rm692e5_visionox_amoled_120hz_create_backlight(dsi);
	if (IS_ERR(ctx->panel.backlight))
		return dev_err_probe(dev, PTR_ERR(ctx->panel.backlight),
				     "Failed to create backlight\n");

	drm_panel_add(&ctx->panel);

	/* This panel only supports DSC; unconditionally enable it */
	dsi->dsc = &ctx->dsc;

	ctx->dsc.dsc_version_major = 1;
	ctx->dsc.dsc_version_minor = 1;

	/* TODO: Pass slice_per_pkt = 2 */
	ctx->dsc.slice_height = 20;
	ctx->dsc.slice_width = 540;
	/*
	 * TODO: hdisplay should be read from the selected mode once
	 * it is passed back to drm_panel (in prepare?)
	 */
	WARN_ON(1080 % ctx->dsc.slice_width);
	ctx->dsc.slice_count = 1080 / ctx->dsc.slice_width;
	ctx->dsc.bits_per_component = 10;
	ctx->dsc.bits_per_pixel = 8 << 4; /* 4 fractional bits */
	ctx->dsc.block_pred_enable = true;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static void rm692e5_visionox_amoled_120hz_remove(struct mipi_dsi_device *dsi)
{
	struct rm692e5_visionox_amoled_120hz *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id rm692e5_visionox_amoled_120hz_of_match[] = {
	{ .compatible = "mdss,rm692e5-visionox-amoled-120hz" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rm692e5_visionox_amoled_120hz_of_match);

static struct mipi_dsi_driver rm692e5_visionox_amoled_120hz_driver = {
	.probe = rm692e5_visionox_amoled_120hz_probe,
	.remove = rm692e5_visionox_amoled_120hz_remove,
	.driver = {
		.name = "panel-rm692e5-visionox-amoled-120hz",
		.of_match_table = rm692e5_visionox_amoled_120hz_of_match,
	},
};
module_mipi_dsi_driver(rm692e5_visionox_amoled_120hz_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for rm692e5 amoled fhd+ 120hz cmd mode dsi visionox panel");
MODULE_LICENSE("GPL");
