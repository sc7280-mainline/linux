// SPDX-License-Identifier: GPL-2.0
/*
 * aw99703 - Backlight driver for the awinic AW99703
 *
 * Copyright (C) 2026 Luka Panio <lukapanio@gmail.com>
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/backlight.h>
#include <linux/regmap.h>
#include <linux/gpio/consumer.h>

#define AW99703_REG_ID       0x00
#define AW99703_REG_MODE     0x02
#define AW99703_REG_LEDCUR   0x03
#define AW99703_REG_LEDLSB   0x06
#define AW99703_REG_LEDMSB   0x07
#define AW99703_REG_FLAGS1   0x0E

#define AW99703_DEFAULT_BRIGHTNESS 255

static const struct regmap_config regmap_config_aw = {
	.reg_bits = 8,
	.val_bits = 8,
};

struct aw99703_data {
	struct i2c_client *client;
	struct regmap *regmap;
	struct backlight_device *bl_dev;
	struct gpio_desc	*en_gpio;
};

static int aw99703_update_brightness(struct backlight_device *bl_dev)
{
	struct aw99703_data *data = bl_get_data(bl_dev);
	unsigned int brightness = backlight_get_brightness(bl_dev);
	int ret;

	ret = regmap_write(data->regmap, AW99703_REG_LEDLSB, brightness & 0x07);
	if (ret) {
		dev_err(&data->client->dev, "Failed to write LSB brightness: %d\n", ret);
		return ret;
	}

	ret = regmap_write(data->regmap, AW99703_REG_LEDMSB, brightness >> 3);
	if (ret) {
		dev_err(&data->client->dev, "Failed to write MSB brightness: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct backlight_ops aw99703_bl_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = aw99703_update_brightness,
};

static int aw99703_probe(struct i2c_client *client)
{
	struct aw99703_data *data;
	struct backlight_properties props;
	int ret;
	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);

	if (!data)
		return -ENOMEM;

	data->en_gpio = devm_gpiod_get(&client->dev, "enable", GPIOD_OUT_HIGH);

	data->client = client;

	data->regmap = devm_regmap_init_i2c(client, &regmap_config_aw);
	if (IS_ERR(data->regmap)) {
		return dev_err_probe(&client->dev, PTR_ERR(data->regmap),
				     "Failed to init regmap\n");
	}

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = 255;
	props.brightness = 255;
	props.scale = BACKLIGHT_SCALE_LINEAR;

	data->bl_dev = devm_backlight_device_register(&client->dev, "aw99703_bl", &client->dev, data, &aw99703_bl_ops, &props);
	if (IS_ERR(data->bl_dev)) {
		return dev_err_probe(&client->dev, PTR_ERR(data->bl_dev),
				     "Failed to register backlight device\n");
	}

	data->bl_dev->props.brightness = AW99703_DEFAULT_BRIGHTNESS;
	i2c_set_clientdata(client, data);

	ret = aw99703_update_brightness(data->bl_dev);
	if (ret) {
		return dev_err_probe(&client->dev, ret,
				     "Failed to set default brightness\n");
	}

	return 0;
}

static const struct i2c_device_id aw99703_id[] = {
	{ "aw99703", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, aw99703_id);

static const struct of_device_id aw99703_of_match[] = {
	{ .compatible = "awinic,aw99703" },
	{}
};
MODULE_DEVICE_TABLE(of, aw99703_of_match);

static struct i2c_driver aw99703_driver = {
	.driver = {
		.name = "aw99703",
		.of_match_table = aw99703_of_match,
	},
	.probe = aw99703_probe,
	.id_table = aw99703_id,
};
module_i2c_driver(aw99703_driver);

MODULE_AUTHOR("Luka Panio <lukapanio@gmail.com>");
MODULE_DESCRIPTION("AW99703 Backlight Driver");
MODULE_LICENSE("GPL");
