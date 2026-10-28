// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Gergo Koteles <soyer@irl.hu>
 *
 * Partially based on aw86927 driver
 *	Copyright (C) 2025 Griffin Kroah-Hartman <griffin.kroah@fairphone.com>
 *
 * Partially based on vendor driver:
 *	Copyright (c) 2021 AWINIC Technology CO., LTD
 *
 */

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/types.h>

#define AW86224_RSTCFG_REG			0x00
#define AW86224_RSTCFG_SOFTRST			0xaa

#define AW86224_SYSINT_REG			0x02
#define AW86224_SYSINT_UVLI			BIT(5)
#define AW86224_SYSINT_OCDI			BIT(2)
#define AW86224_SYSINT_OTI			BIT(1)

#define AW86224_SYSINTM_REG			0x03
#define AW86224_SYSINTM_FF_AEM			BIT(4)
#define AW86224_SYSINTM_FF_AFM			BIT(3)
#define AW86224_SYSINTM_DONEM			BIT(0)

#define AW86224_PLAYCFG2_REG			0x07
#define AW86224_PLAYCFG2_GAIN_MAX		0x80

#define AW86224_PLAYCFG3_REG			0x08
#define AW86224_PLAYCFG3_PLAY_MODE		GENMASK(1, 0)
#define AW86224_PLAYCFG3_PLAY_MODE_RAM		0

#define AW86224_PLAYCFG4_REG			0x09
#define AW86224_PLAYCFG4_STOP			BIT(1)
#define AW86224_PLAYCFG4_GO			BIT(0)

#define AW86224_WAVCFG9_REG			0x12
#define AW86224_WAVCFG9_SEQ1LOOP		GENMASK(7, 4)
#define AW86224_WAVCFG9_SEQ1LOOP_INFINITELY	0x0f

#define AW86224_GLBRD5_REG			0x3f
#define AW86224_GLBRD5_STATE			GENMASK(3, 0)
#define AW86224_GLBRD5_STATE_STANDBY		0

#define AW86224_RAMADDRH_REG			0x40
#define AW86224_RAMADDRH_MASK			GENMASK(3, 0)

#define AW86224_RAMADDRL_REG			0x41

#define AW86224_RAMDATA_REG			0x42

#define AW86224_SYSCTRL1_REG			0x43
#define AW86224_SYSCTRL1_EN_RAMINIT		BIT(3)

#define AW86224_SYSCTRL2_REG			0x44
#define AW86224_SYSCTRL2_STANDBY		BIT(6)

#define AW86224_SYSCTRL7_REG			0x49
#define AW86224_SYSCTRL7_GAIN_BYPASS		BIT(6)

#define AW86224_CHIPID_REG			0x64
#define AW86224_CHIPID_H			BIT(6)
#define AW86224_CHIPID_H_AW8622X		0

/* default value of RAM mode base addr */
#define AW86224_RAM_BASE_ADDR			0x800
#define AW86224_RAM_BASE_ADDR_H			(AW86224_RAM_BASE_ADDR >> 8)
#define AW86224_RAM_BASE_ADDR_L			(AW86224_RAM_BASE_ADDR & 0xff)

enum aw86224_work_mode {
	AW86224_STANDBY_MODE,
	AW86224_RAM_MODE,
};

struct aw86224_data {
	struct work_struct play_work;
	struct device *dev;
	struct input_dev *input_dev;
	struct i2c_client *client;
	struct regmap *regmap;
	struct gpio_desc *reset_gpio;
	u16 level;
	bool running;
	bool suspend;
};

static const struct regmap_range aw86224_volatile_registers[] = {
	regmap_reg_range(AW86224_SYSINT_REG, AW86224_SYSINT_REG),
	regmap_reg_range(AW86224_GLBRD5_REG, AW86224_GLBRD5_REG),
	regmap_reg_range(AW86224_RAMDATA_REG, AW86224_RAMDATA_REG),
};

static const struct regmap_access_table aw86224_volatile_table = {
	.yes_ranges = aw86224_volatile_registers,
	.n_yes_ranges = ARRAY_SIZE(aw86224_volatile_registers),
};

static const struct reg_default aw86224_regmap_defs[] = {
	{ AW86224_PLAYCFG2_REG,		0x80 },
	{ AW86224_PLAYCFG3_REG,		0x04 },
	{ AW86224_PLAYCFG4_REG,		0x00 },
	{ AW86224_WAVCFG9_REG,		0x00 },
	{ AW86224_RAMADDRH_REG,		0x00 },
	{ AW86224_RAMADDRL_REG,		0x00 },
	{ AW86224_SYSCTRL1_REG,		0x04 },
	{ AW86224_SYSCTRL2_REG,		0x28 },
	{ AW86224_SYSCTRL7_REG,		0x14 },
};

static const struct regmap_config aw86224_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x80,
	.cache_type = REGCACHE_MAPLE,
	.reg_defaults = aw86224_regmap_defs,
	.num_reg_defaults = ARRAY_SIZE(aw86224_regmap_defs),
	.volatile_table = &aw86224_volatile_table,
};

/*
 * Sine wave representing the magnitude of the drive to be used.
 * Data is encoded in two's complement.
 *   round(84 * sin(x / 16.25))
 */
static const u8 aw86224_waveform[] = {
	0x00, 0x05, 0x0a, 0x0f, 0x14, 0x1a, 0x1f, 0x23, 0x28, 0x2d, 0x31, 0x35,
	0x39, 0x3d, 0x41, 0x44, 0x47, 0x4a, 0x4c, 0x4f, 0x51, 0x52, 0x53, 0x54,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x54, 0x52, 0x51, 0x4f, 0x4d, 0x4a, 0x47,
	0x44, 0x41, 0x3d, 0x3a, 0x36, 0x31, 0x2d, 0x28, 0x24, 0x1f, 0x1a, 0x15,
	0x10, 0x0a, 0x05, 0x00, 0xfc, 0xf6, 0xf1, 0xec, 0xe7, 0xe2, 0xdd, 0xd8,
	0xd4, 0xcf, 0xcb, 0xc7, 0xc3, 0xbf, 0xbc, 0xb9, 0xb6, 0xb4, 0xb1, 0xb0,
	0xae, 0xad, 0xac, 0xab, 0xab, 0xab, 0xab, 0xab, 0xac, 0xae, 0xaf, 0xb1,
	0xb3, 0xb6, 0xb8, 0xbc, 0xbf, 0xc2, 0xc6, 0xca, 0xce, 0xd3, 0xd7, 0xdc,
	0xe1, 0xe6, 0xeb, 0xf0, 0xf5, 0xfb
};

struct aw86224_sram_waveform_header {
	u8 version;
	__be16 start_address;
	__be16 end_address;
} __packed;

static const struct aw86224_sram_waveform_header sram_waveform_header = {
	.version = 0x01,
	.start_address = cpu_to_be16(AW86224_RAM_BASE_ADDR +
			sizeof(struct aw86224_sram_waveform_header)),
	.end_address = cpu_to_be16(AW86224_RAM_BASE_ADDR +
			sizeof(struct aw86224_sram_waveform_header) +
			ARRAY_SIZE(aw86224_waveform) - 1),
};

static int aw86224_wait_enter_standby(struct aw86224_data *haptics)
{
	unsigned int reg_val;
	int err;

	err = regmap_read_poll_timeout(haptics->regmap, AW86224_GLBRD5_REG, reg_val,
				       (FIELD_GET(AW86224_GLBRD5_STATE, reg_val) ==
				       AW86224_GLBRD5_STATE_STANDBY),
				       2500, 2500 * 100);
	if (err) {
		dev_err(haptics->dev, "did not enter standby: %d\n", err);
		return err;
	}

	return 0;
}

static int aw86224_play_mode(struct aw86224_data *haptics, u8 play_mode)
{
	int err;

	switch (play_mode) {
	case AW86224_STANDBY_MODE:
		/* Briefly toggle standby, then toggle back to standby off */
		err = regmap_set_bits(haptics->regmap,
				      AW86224_SYSCTRL2_REG,
				      AW86224_SYSCTRL2_STANDBY);
		if (err)
			return err;

		err = regmap_clear_bits(haptics->regmap,
					AW86224_SYSCTRL2_REG,
					AW86224_SYSCTRL2_STANDBY);
		if (err)
			return err;

		break;

	case AW86224_RAM_MODE:
		err = regmap_update_bits(haptics->regmap,
					 AW86224_PLAYCFG3_REG,
					 AW86224_PLAYCFG3_PLAY_MODE,
					 FIELD_PREP(AW86224_PLAYCFG3_PLAY_MODE,
						    AW86224_PLAYCFG3_PLAY_MODE_RAM));
		if (err)
			return err;

		break;
	}

	return 0;
}

static int aw86224_stop(struct aw86224_data *haptics)
{
	int err;

	err = regmap_write(haptics->regmap, AW86224_PLAYCFG4_REG, AW86224_PLAYCFG4_STOP);
	if (err) {
		dev_err(haptics->dev, "Failed to stop playback: %d\n", err);
		return err;
	}

	err = aw86224_wait_enter_standby(haptics);
	if (err) {
		dev_err(haptics->dev, "Failed to enter standby, trying to force it\n");
		err = aw86224_play_mode(haptics, AW86224_STANDBY_MODE);
		if (err)
			return err;
	}

	haptics->running = false;

	return 0;
}

static int aw86224_haptics_play(struct input_dev *dev, void *data, struct ff_effect *effect)
{
	struct aw86224_data *haptics = input_get_drvdata(dev);
	int level;

	level = effect->u.rumble.strong_magnitude;
	if (!level)
		level = effect->u.rumble.weak_magnitude;

	haptics->level = level;

	schedule_work(&haptics->play_work);

	return 0;
}

static int aw86224_update_gain(struct aw86224_data *haptics)
{
	return regmap_write(haptics->regmap,
			    AW86224_PLAYCFG2_REG,
			    haptics->level * AW86224_PLAYCFG2_GAIN_MAX / USHRT_MAX);
}

static int aw86224_play_sine(struct aw86224_data *haptics)
{
	int err;

	err = aw86224_play_mode(haptics, AW86224_RAM_MODE);
	if (err)
		return err;

	err = regmap_update_bits(haptics->regmap,
				 AW86224_WAVCFG9_REG,
				 AW86224_WAVCFG9_SEQ1LOOP,
				 FIELD_PREP(AW86224_WAVCFG9_SEQ1LOOP,
					    AW86224_WAVCFG9_SEQ1LOOP_INFINITELY));
	if (err)
		return err;

	err = aw86224_update_gain(haptics);
	if (err)
		return err;

	err = regmap_write(haptics->regmap, AW86224_PLAYCFG4_REG, AW86224_PLAYCFG4_GO);
	if (err)
		return err;

	haptics->running = true;

	return 0;
}

static void aw86224_close(struct input_dev *input)
{
	struct aw86224_data *haptics = input_get_drvdata(input);
	struct device *dev = &haptics->client->dev;
	int err;

	cancel_work_sync(&haptics->play_work);

	err = aw86224_stop(haptics);
	if (err)
		dev_err(dev, "Failed to close the Driver: %d\n", err);
}

static void aw86224_haptics_play_work(struct work_struct *work)
{
	struct aw86224_data *haptics =
		container_of(work, struct aw86224_data, play_work);
	struct device *dev = &haptics->client->dev;
	int err;

	if (haptics->level)
		if (!haptics->running)
			err = aw86224_play_sine(haptics);
		else
			err = aw86224_update_gain(haptics);
	else
		err = aw86224_stop(haptics);

	if (err)
		dev_err(dev, "Failed to execute work command: %d\n", err);
}

static void aw86224_hw_reset(struct aw86224_data *haptics)
{
	/* Assert reset */
	gpiod_set_value_cansleep(haptics->reset_gpio, 1);
	/* Wait ~1ms */
	usleep_range(1000, 2000);
	/* Deassert reset */
	gpiod_set_value_cansleep(haptics->reset_gpio, 0);
	/* Wait ~8ms until I2C is accessible */
	usleep_range(8000, 8500);
}

static int aw86224_haptic_init(struct aw86224_data *haptics)
{
	int err;

	err = regmap_set_bits(haptics->regmap,
			      AW86224_SYSCTRL7_REG,
			      AW86224_SYSCTRL7_GAIN_BYPASS);
	if (err)
		return err;

	return 0;
}

static int aw86224_ram_init(struct aw86224_data *haptics)
{
	int err;

	err = aw86224_wait_enter_standby(haptics);
	if (err)
		return err;

	err = regmap_set_bits(haptics->regmap,
			      AW86224_SYSCTRL1_REG,
			      AW86224_SYSCTRL1_EN_RAMINIT);
	if (err)
		return err;

	/* AW86224 wants a 1ms delay here */
	usleep_range(1000, 1500);

	/* Set start of SRAM, before the data is written it will be the same as the base */
	err = regmap_update_bits(haptics->regmap,
				 AW86224_RAMADDRH_REG,
				 AW86224_RAMADDRH_MASK,
				 FIELD_PREP(AW86224_RAMADDRH_MASK,
					    AW86224_RAM_BASE_ADDR_H));
	if (err)
		return err;

	err = regmap_write(haptics->regmap,
			   AW86224_RAMADDRL_REG, AW86224_RAM_BASE_ADDR_L);
	if (err)
		return err;

	/* Write waveform header to SRAM */
	err = regmap_noinc_write(haptics->regmap, AW86224_RAMDATA_REG,
				 &sram_waveform_header, sizeof(sram_waveform_header));
	if (err)
		return err;

	/* Write waveform to SRAM */
	err = regmap_noinc_write(haptics->regmap, AW86224_RAMDATA_REG,
				 aw86224_waveform, ARRAY_SIZE(aw86224_waveform));
	if (err)
		return err;

	/* Disable SRAM init */
	err = regmap_clear_bits(haptics->regmap,
				AW86224_SYSCTRL1_REG,
				AW86224_SYSCTRL1_EN_RAMINIT);
	if (err)
		return err;

	return 0;
}

static irqreturn_t aw86224_irq(int irq, void *data)
{
	struct aw86224_data *haptics = data;
	struct device *dev = &haptics->client->dev;
	unsigned int reg_val;
	int err;

	err = regmap_read(haptics->regmap, AW86224_SYSINT_REG, &reg_val);
	if (err) {
		dev_err(dev, "Failed to read SYSINT register: %d\n", err);
		return IRQ_NONE;
	}

	if (reg_val & AW86224_SYSINT_UVLI)
		dev_err(dev, "Received an Under Voltage Lock Out interrupt\n");
	if (reg_val & AW86224_SYSINT_OCDI)
		dev_err(dev, "Received an Over Current interrupt\n");
	if (reg_val & AW86224_SYSINT_OTI)
		dev_err(dev, "Received an Over Temperature interrupt\n");

	return IRQ_HANDLED;
}

static int aw86224_detect(struct aw86224_data *haptics)
{
	unsigned int chip_id;
	int err;

	err = regmap_read(haptics->regmap, AW86224_CHIPID_REG, &chip_id);
	if (err)
		return dev_err_probe(haptics->dev, err, "Failed to read CHIPID register\n");

	if ((chip_id & AW86224_CHIPID_H) != AW86224_CHIPID_H_AW8622X) {
		dev_err(haptics->dev, "Unexpected CHIPID value: 0x%x\n", chip_id);
		return -ENODEV;
	}

	return 0;
}

static int aw86224_probe(struct i2c_client *client)
{
	struct aw86224_data *haptics;
	int err;

	haptics = devm_kzalloc(&client->dev, sizeof(struct aw86224_data), GFP_KERNEL);
	if (!haptics)
		return -ENOMEM;

	haptics->dev = &client->dev;
	haptics->client = client;

	i2c_set_clientdata(client, haptics);

	haptics->regmap = devm_regmap_init_i2c(client, &aw86224_regmap_config);
	if (IS_ERR(haptics->regmap))
		return dev_err_probe(haptics->dev, PTR_ERR(haptics->regmap),
				     "Failed to allocate register map\n");

	haptics->input_dev = devm_input_allocate_device(haptics->dev);
	if (!haptics->input_dev)
		return -ENOMEM;

	haptics->reset_gpio = devm_gpiod_get(haptics->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(haptics->reset_gpio))
		return dev_err_probe(haptics->dev, PTR_ERR(haptics->reset_gpio),
				     "Failed to get reset gpio\n");

	/* Hardware reset */
	aw86224_hw_reset(haptics);

	/* Software reset */
	err = regmap_write(haptics->regmap, AW86224_RSTCFG_REG, AW86224_RSTCFG_SOFTRST);
	if (err)
		return dev_err_probe(haptics->dev, err,	"Failed Software reset\n");

	/* Wait ~3ms until I2C is accessible */
	usleep_range(3000, 3500);

	err = aw86224_detect(haptics);
	if (err)
		return dev_err_probe(haptics->dev, err, "Failed to find chip\n");

	/* Disable FF_AE, FF_AF, DONE interrupts */
	err = regmap_write(haptics->regmap,
			   AW86224_SYSINTM_REG,
			   AW86224_SYSINTM_FF_AEM |
			   AW86224_SYSINTM_FF_AFM |
			   AW86224_SYSINTM_DONEM);
	if (err)
		return dev_err_probe(haptics->dev, err, "Failed to configure interrupt masks\n");

	err = devm_request_threaded_irq(haptics->dev, client->irq, NULL,
					aw86224_irq, IRQF_ONESHOT, NULL, haptics);
	if (err)
		return dev_err_probe(haptics->dev, err, "Failed to request threaded irq\n");

	INIT_WORK(&haptics->play_work, aw86224_haptics_play_work);

	haptics->input_dev->name = "aw86224-haptics";
	haptics->input_dev->close = aw86224_close;

	input_set_drvdata(haptics->input_dev, haptics);
	input_set_capability(haptics->input_dev, EV_FF, FF_RUMBLE);

	err = input_ff_create_memless(haptics->input_dev, NULL, aw86224_haptics_play);
	if (err)
		return dev_err_probe(haptics->dev, err, "Failed to create FF dev\n");

	/* Set up registers */
	err = aw86224_play_mode(haptics, AW86224_STANDBY_MODE);
	if (err)
		return dev_err_probe(haptics->dev, err,
				     "Failed to enter standby for Haptic init\n");

	err = aw86224_haptic_init(haptics);
	if (err)
		return dev_err_probe(haptics->dev, err, "Haptic init failed\n");

	/* RAM init, upload the waveform for playback */
	err = aw86224_ram_init(haptics);
	if (err)
		return dev_err_probe(haptics->dev, err, "Failed to init sram\n");

	err = input_register_device(haptics->input_dev);
	if (err)
		return dev_err_probe(haptics->dev, err, "Failed to register input device\n");

	return 0;
}

static int aw86224_suspend(struct device *dev)
{
	struct aw86224_data *haptics = dev_get_drvdata(dev);

	if (haptics->suspend)
		return 0;

	disable_work_sync(&haptics->play_work);
	aw86224_stop(haptics);
	haptics->suspend = true;

	return 0;
}

static int aw86224_resume(struct device *dev)
{
	struct aw86224_data *haptics = dev_get_drvdata(dev);

	if (!haptics->suspend)
		return 0;

	haptics->suspend = false;
	enable_work(&haptics->play_work);

	return 0;
}

static DEFINE_SIMPLE_DEV_PM_OPS(aw86224_pm_ops,
				aw86224_suspend,
				aw86224_resume);

static const struct of_device_id aw86224_of_id[] = {
	{ .compatible = "awinic,aw86224" },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, aw86224_of_id);

static struct i2c_driver aw86224_driver = {
	.driver = {
		.name = "aw86224-haptics",
		.pm   = pm_sleep_ptr(&aw86224_pm_ops),
		.of_match_table = aw86224_of_id,
	},
	.probe = aw86224_probe,
};

module_i2c_driver(aw86224_driver);

MODULE_AUTHOR("Gergo Koteles <soyer@irl.hu>");
MODULE_DESCRIPTION("AWINIC AW86224 LRA Haptic Driver");
MODULE_LICENSE("GPL");
