/*
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, FocalTech Systems, Ltd., all rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#include "focaltech.h"

#define FTS_DRIVER_NAME		"fts_ts"
#define INTERVAL_READ_REG	200 /* unit:ms */
#define TIMEOUT_READ_REG	1000 /* unit:ms */

struct fts_ts_data *fts_data;

static int fts_ts_suspend(struct device *dev);
static int fts_ts_resume(struct device *dev);

int fts_check_cid(struct fts_ts_data *ts_data, u8 id_h)
{
	int i = 0;
	struct ft_chip_id_t *cid = &ts_data->ic_info.cid;
	u8 cid_h = 0x0;

	if (cid->type == 0)
		return -ENODATA;

	for (i = 0; i < FTS_MAX_CHIP_IDS; i++) {
		cid_h = ((cid->chip_ids[i] >> 8) & 0x00FF);
		if (cid_h && (id_h == cid_h))
			return 0;
	}

	return -ENODATA;
}

/*****************************************************************************
*  Name: fts_wait_tp_to_valid
*  Brief: Read chip id until TP FW become valid(Timeout: TIMEOUT_READ_REG),
*  	 need call when reset/power on/resume...
*  Input:
*  Output:
*  Return: return 0 if tp valid, otherwise return error code
*****************************************************************************/
int fts_wait_tp_to_valid(void)
{
	int ret = 0;
	int cnt = 0;
	u8 idh = 0;
	struct fts_ts_data *ts_data = fts_data;
	u8 chip_idh = ts_data->ic_info.ids.chip_idh;

	do {
		ret = fts_read_reg(FTS_REG_CHIP_ID, &idh);
		if ((idh == chip_idh) || (fts_check_cid(ts_data, idh) == 0)) {
			dev_info(ts_data->dev, "TP Ready,Device ID:0x%02x",
				 idh);
			return 0;
		} else
			dev_dbg(ts_data->dev,
				"TP Not Ready,ReadData:0x%02x,ret:%d", idh,
				ret);

		cnt++;
		msleep(INTERVAL_READ_REG);
	} while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

	return -EIO;
}

int fts_request_handle_reset(struct fts_ts_data *ts_data, int hdelayms)
{
	gpiod_set_value_cansleep(ts_data->reset_gpio, 0);
	usleep_range(1000, 1100);
	gpiod_set_value_cansleep(ts_data->reset_gpio, 1);

	if (hdelayms)
		msleep(hdelayms);

	ts_data->fod_info.fp_down_report = 0;

	return 0;
}

void fts_irq_disable(void)
{
	unsigned long irqflags;

	spin_lock_irqsave(&fts_data->irq_lock, irqflags);

	if (!fts_data->irq_disabled) {
		disable_irq_nosync(fts_data->irq);
		fts_data->irq_disabled = true;
	}

	spin_unlock_irqrestore(&fts_data->irq_lock, irqflags);
}

void fts_irq_enable(void)
{
	unsigned long irqflags = 0;

	spin_lock_irqsave(&fts_data->irq_lock, irqflags);

	if (fts_data->irq_disabled) {
		enable_irq(fts_data->irq);
		fts_data->irq_disabled = false;
	}

	spin_unlock_irqrestore(&fts_data->irq_lock, irqflags);
}

void fts_hid2std(void)
{
	int ret = 0;
	u8 buf[3] = { 0xEB, 0xAA, 0x09 };

	if (fts_data->bus_type != BUS_TYPE_I2C)
		return;

	ret = fts_write(buf, 3);
	if (ret < 0)
		dev_err(fts_data->dev, "hid2std cmd write fail");
	else {
		msleep(10);
		buf[0] = buf[1] = buf[2] = 0;
		ret = fts_read(NULL, 0, buf, 3);
		if (ret < 0)
			dev_err(fts_data->dev, "hid2std cmd read fail");
		else if ((0xEB == buf[0]) && (0xAA == buf[1]) &&
			 (0x08 == buf[2]))
			dev_dbg(fts_data->dev,
				"hidi2c change to stdi2c successful");
		else
			dev_dbg(fts_data->dev,
				"hidi2c change to stdi2c not support or fail");
	}
}

static int fts_get_chip_types(struct fts_ts_data *ts_data, u8 id_h, u8 id_l,
			      bool fw_valid)
{
	u32 i = 0;
	struct ft_chip_t ctype[] = FTS_CHIP_TYPE_MAPPING;
	u32 ctype_entries = sizeof(ctype) / sizeof(struct ft_chip_t);

	if ((0x0 == id_h) || (0x0 == id_l)) {
		dev_dbg(ts_data->dev, "id_h/id_l is 0");
		return -EINVAL;
	}

	dev_info(ts_data->dev, "verify id:0x%02x%02x", id_h, id_l);
	for (i = 0; i < ctype_entries; i++) {
		if (VALID == fw_valid) {
			if (((id_h == ctype[i].chip_idh) &&
			     (id_l == ctype[i].chip_idl)))
				break;
		} else {
			if (((id_h == ctype[i].rom_idh) &&
			     (id_l == ctype[i].rom_idl)) ||
			    ((id_h == ctype[i].pb_idh) &&
			     (id_l == ctype[i].pb_idl)) ||
			    ((id_h == ctype[i].bl_idh) &&
			     (id_l == ctype[i].bl_idl))) {
				break;
			}
		}
	}

	if (i >= ctype_entries)
		return -ENODATA;

	ts_data->ic_info.ids = ctype[i];
	return 0;
}

static int fts_read_bootid(struct fts_ts_data *ts_data, u8 *id)
{
	int ret = 0;
	u8 chip_id[2] = { 0 };
	u8 id_cmd[4] = { 0 };
	u32 id_cmd_len = 0;

	id_cmd[0] = FTS_CMD_START1;
	id_cmd[1] = FTS_CMD_START2;
	ret = fts_write(id_cmd, 2);
	if (ret < 0) {
		dev_err(ts_data->dev, "start cmd write fail");
		return ret;
	}

	msleep(FTS_CMD_START_DELAY);
	id_cmd[0] = FTS_CMD_READ_ID;
	id_cmd[1] = id_cmd[2] = id_cmd[3] = 0x00;
	if (ts_data->ic_info.is_incell)
		id_cmd_len = FTS_CMD_READ_ID_LEN_INCELL;
	else
		id_cmd_len = FTS_CMD_READ_ID_LEN;
	ret = fts_read(id_cmd, id_cmd_len, chip_id, 2);
	if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
		dev_err(ts_data->dev, "read boot id fail,read:0x%02x%02x",
			chip_id[0], chip_id[1]);
		return -EIO;
	}

	id[0] = chip_id[0];
	id[1] = chip_id[1];
	return 0;
}

/*****************************************************************************
* Name: fts_get_ic_information
* Brief: read chip id to get ic information, after run the function, driver w-
*		ill know which IC is it.
*		If cant get the ic information, maybe not focaltech's touch IC, need
*		unregister the driver
* Input:
* Output:
* Return: return 0 if get correct ic information, otherwise return error code
*****************************************************************************/
static int fts_get_ic_information(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int cnt = 0;
	u8 chip_id[2] = { 0 };

	ts_data->ic_info.is_incell = FTS_CHIP_IDC;
	ts_data->ic_info.hid_supported = FTS_HID_SUPPORTTED;

	for (cnt = 0; cnt < 3; cnt++) {
		fts_request_handle_reset(ts_data, 0);
		mdelay(FTS_CMD_START_DELAY + (cnt * 8));

		ret = fts_read_bootid(ts_data, &chip_id[0]);
		if (ret < 0) {
			dev_dbg(ts_data->dev, "read boot id fail,retry:%d",
				cnt);
			continue;
		}

		ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1],
					 INVALID);
		if (ret < 0) {
			dev_dbg(ts_data->dev,
				"can't get ic informaton,retry:%d", cnt);
			continue;
		}

		break;
	}

	if (cnt >= 3) {
		dev_err(ts_data->dev, "get ic informaton fail");
		return -EIO;
	}

	dev_info(ts_data->dev,
		 "get ic information, chip id = 0x%02x%02x(cid type=0x%x)",
		 ts_data->ic_info.ids.chip_idh, ts_data->ic_info.ids.chip_idl,
		 ts_data->ic_info.cid.type);

	return 0;
}

/*****************************************************************************
*  Reprot related
*****************************************************************************/
static void fts_show_touch_buffer(u8 *data, u32 datalen)
{
	u32 i = 0;
	u32 count = 0;
	char *tmpbuf = NULL;

	tmpbuf = devm_kzalloc(fts_data->dev, 1024, GFP_KERNEL);
	if (!tmpbuf)
		return;

	for (i = 0; i < datalen; i++) {
		count += snprintf(tmpbuf + count, 1024 - count, "%02X,",
				  data[i]);
		if (count >= 1024)
			break;
	}
	dev_dbg(fts_data->dev, "touch_buf:%s", tmpbuf);

	if (tmpbuf)
		tmpbuf = NULL;
}

void fts_release_all_finger(void)
{
	struct fts_ts_data *ts_data = fts_data;
	struct input_dev *input_dev = ts_data->input_dev;
	u32 finger_count = 0;
	u32 max_touches = ts_data->pdata->max_touch_number;

	mutex_lock(&ts_data->report_mutex);
	for (finger_count = 0; finger_count < max_touches; finger_count++) {
		input_mt_slot(input_dev, finger_count);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
	}

	input_report_key(input_dev, BTN_TOUCH, 0);
	input_sync(input_dev);

	ts_data->touch_points = 0;
	ts_data->key_state = 0;
	mutex_unlock(&ts_data->report_mutex);
}

static int fts_input_report_b(struct fts_ts_data *ts_data,
			      struct ts_event *events)
{
	int i = 0;
	int touch_down_point_cur = 0;
	int touch_point_pre = ts_data->touch_points;
	u32 max_touch_num = ts_data->pdata->max_touch_number;
	bool touch_event_coordinate = false;
	struct input_dev *input_dev = ts_data->input_dev;

	for (i = 0; i < ts_data->touch_event_num; i++) {
		touch_event_coordinate = true;
		if (EVENT_DOWN(events[i].flag)) {
			input_mt_slot(input_dev, events[i].id);
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER,
						   true);
			input_report_abs(input_dev, ABS_MT_PRESSURE,
					 events[i].p);
			input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR,
					 events[i].area);
			input_report_abs(input_dev, ABS_MT_POSITION_X,
					 events[i].x);
			input_report_abs(input_dev, ABS_MT_POSITION_Y,
					 events[i].y);

			touch_down_point_cur |= (1 << events[i].id);
			touch_point_pre |= (1 << events[i].id);

			if (FTS_TOUCH_DOWN == events[i].flag) {
				dev_dbg(ts_data->dev,
					"[B]P%d(%d, %d)[p:%d,tm:%d] DOWN!",
					events[i].id, events[i].x, events[i].y,
					events[i].p, events[i].area);
			}
		} else {
			input_mt_slot(input_dev, events[i].id);
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER,
						   false);
			touch_point_pre &= ~(1 << events[i].id);
			dev_dbg(ts_data->dev, "[B]P%d UP!", events[i].id);
		}
	}

	if (unlikely(touch_point_pre ^ touch_down_point_cur)) {
		for (i = 0; i < max_touch_num; i++) {
			if ((1 << i) &
			    (touch_point_pre ^ touch_down_point_cur)) {
				dev_dbg(ts_data->dev, "[B]P%d UP!", i);
				input_mt_slot(input_dev, i);
				input_mt_report_slot_state(
					input_dev, MT_TOOL_FINGER, false);
			}
		}
	}

	if (touch_down_point_cur)
		input_report_key(input_dev, BTN_TOUCH, 1);
	else if (touch_event_coordinate || ts_data->touch_points) {
		if (ts_data->touch_points)
			dev_dbg(ts_data->dev, "[B]Points All Up!");
		input_report_key(input_dev, BTN_TOUCH, 0);
	}

	ts_data->touch_points = touch_down_point_cur;
	input_sync(input_dev);
	return 0;
}

static int fts_read_touchdata(struct fts_ts_data *ts_data, u8 *buf)
{
	int ret = 0;

	ts_data->touch_addr = 0x01;
	ret = fts_read(&ts_data->touch_addr, 1, buf, ts_data->touch_size);

	if (((0xEF == buf[1]) && (0xEF == buf[2]) && (0xEF == buf[3])) ||
	    ((ret < 0) && (0xEF == buf[0]))) {
		fts_release_all_finger();
		/* check if need recovery fw */
		fts_fw_recovery();
		ts_data->fw_is_running = true;
		return 1;
	} else if (ret < 0) {
		dev_err(ts_data->dev, "touch data(%x) abnormal,ret:%d", buf[1],
			ret);
		return ret;
	}

	return 0;
}

static int fts_read_parse_touchdata(struct fts_ts_data *ts_data, u8 *touch_buf)
{
	int ret = 0;

	memset(touch_buf, 0xFF, FTS_MAX_TOUCH_BUF);
	ts_data->ta_size = ts_data->touch_size;

	/*read touch data*/
	ret = fts_read_touchdata(ts_data, touch_buf);
	if (ret < 0) {
		dev_err(ts_data->dev, "read touch data fails");
		return TOUCH_ERROR;
	}

	fts_show_touch_buffer(touch_buf, ts_data->ta_size);

	if (ret)
		return TOUCH_IGNORE;

	if ((touch_buf[1] == 0xFF) && (touch_buf[2] == 0xFF) &&
	    (touch_buf[3] == 0xFF) && (touch_buf[4] == 0xFF)) {
		dev_info(ts_data->dev,
			 "touch buff is 0xff, need recovery state");
		return TOUCH_FW_INIT;
	}

	return ((touch_buf[FTS_TOUCH_E_NUM] >> 4) & 0x0F);
}

static int fts_irq_read_report(struct fts_ts_data *ts_data)
{
	int i = 0;
	int max_touch_num = ts_data->pdata->max_touch_number;
	int touch_etype = 0;
	u8 event_num = 0;
	u8 finger_num = 0;
	u8 pointid = 0;
	u8 base = 0;
	u8 *touch_buf = ts_data->touch_buf;
	struct ts_event *events = ts_data->events;

	touch_etype = fts_read_parse_touchdata(ts_data, touch_buf);
	switch (touch_etype) {
	case TOUCH_DEFAULT:
		finger_num = touch_buf[FTS_TOUCH_E_NUM] & 0x0F;
		if (finger_num > max_touch_num) {
			dev_err(ts_data->dev, "invalid point_num(%d)",
				finger_num);
			return -EIO;
		}

		for (i = 0; i < max_touch_num; i++) {
			base = FTS_ONE_TCH_LEN * i + 2;
			pointid = (touch_buf[FTS_TOUCH_OFF_ID_YH + base]) >> 4;
			if (pointid >= FTS_MAX_ID)
				break;
			else if (pointid >= max_touch_num) {
				dev_err(ts_data->dev,
					"ID(%d) beyond max_touch_number",
					pointid);
				return -EINVAL;
			}

			events[i].id = pointid;
			events[i].flag = touch_buf[FTS_TOUCH_OFF_E_XH + base] >>
					 6;
			events[i].x =
				((touch_buf[FTS_TOUCH_OFF_E_XH + base] & 0x0F)
				 << 8) +
				(touch_buf[FTS_TOUCH_OFF_XL + base] & 0xFF);
			events[i].y =
				((touch_buf[FTS_TOUCH_OFF_ID_YH + base] & 0x0F)
				 << 8) +
				(touch_buf[FTS_TOUCH_OFF_YL + base] & 0xFF);
			events[i].p = touch_buf[FTS_TOUCH_OFF_PRE + base];
			events[i].area = touch_buf[FTS_TOUCH_OFF_AREA + base];
			if (events[i].p <= 0)
				events[i].p = 0x3F;
			if (events[i].area <= 0)
				events[i].area = 0x09;

			event_num++;
			if (EVENT_DOWN(events[i].flag) && (finger_num == 0)) {
				dev_info(ts_data->dev,
					 "abnormal touch data from fw");
				return -EIO;
			}
		}

		if (event_num == 0) {
			dev_info(ts_data->dev,
				 "no touch point information(%02x)",
				 touch_buf[2]);
			return -EIO;
		}
		ts_data->touch_event_num = event_num;

		mutex_lock(&ts_data->report_mutex);
		fts_input_report_b(ts_data, events);
		mutex_unlock(&ts_data->report_mutex);
		break;

	case TOUCH_EVENT_NUM:
		event_num = touch_buf[FTS_TOUCH_E_NUM] & 0x0F;
		if (!event_num || (event_num > max_touch_num)) {
			dev_err(ts_data->dev, "invalid touch event num(%d)",
				event_num);
			return -EIO;
		}

		ts_data->touch_event_num = event_num;
		for (i = 0; i < event_num; i++) {
			base = FTS_ONE_TCH_LEN * i + 2;
			pointid = (touch_buf[FTS_TOUCH_OFF_ID_YH + base]) >> 4;
			if (pointid >= max_touch_num) {
				dev_err(ts_data->dev,
					"touch point ID(%d) beyond max_touch_number(%d)",
					pointid, max_touch_num);
				return -EINVAL;
			}

			events[i].id = pointid;
			events[i].flag = touch_buf[FTS_TOUCH_OFF_E_XH + base] >>
					 6;
			events[i].x =
				((touch_buf[FTS_TOUCH_OFF_E_XH + base] & 0x0F)
				 << 8) +
				(touch_buf[FTS_TOUCH_OFF_XL + base] & 0xFF);
			events[i].y =
				((touch_buf[FTS_TOUCH_OFF_ID_YH + base] & 0x0F)
				 << 8) +
				(touch_buf[FTS_TOUCH_OFF_YL + base] & 0xFF);
			events[i].p = touch_buf[FTS_TOUCH_OFF_PRE + base];
			events[i].area = touch_buf[FTS_TOUCH_OFF_AREA + base];
			if (events[i].p <= 0)
				events[i].p = 0x3F;
			if (events[i].area <= 0)
				events[i].area = 0x09;
		}

		mutex_lock(&ts_data->report_mutex);
		fts_input_report_b(ts_data, events);
		mutex_unlock(&ts_data->report_mutex);
		break;

	case TOUCH_FW_INIT:
		fts_release_all_finger();
		fts_wait_tp_to_valid();
		break;

	case TOUCH_IGNORE:
	case TOUCH_ERROR:
		break;

	default:
		dev_info(ts_data->dev, "unknown touch event(%d)", touch_etype);
		break;
	}

	return 0;
}

static irqreturn_t fts_irq_handler(int irq, void *data)
{
	struct fts_ts_data *ts_data = fts_data;

	ts_data->intr_jiffies = jiffies;
	fts_irq_read_report(ts_data);

	return IRQ_HANDLED;
}

static int fts_irq_registration(struct fts_ts_data *ts_data)
{
	int ret = 0;
	struct fts_ts_platform_data *pdata = ts_data->pdata;

	ts_data->irq = gpio_to_irq(pdata->irq_gpio);
	pdata->irq_gpio_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
	dev_info(ts_data->dev, "irq:%d, flag:%x", ts_data->irq,
		 pdata->irq_gpio_flags);
	ret = request_threaded_irq(ts_data->irq, NULL, fts_irq_handler,
				   pdata->irq_gpio_flags, FTS_DRIVER_NAME,
				   ts_data);

	return ret;
}

static int fts_input_init(struct fts_ts_data *ts_data)
{
	int ret = 0;
	struct fts_ts_platform_data *pdata = ts_data->pdata;
	struct input_dev *input_dev;

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(ts_data->dev,
			"Failed to allocate memory for input device");
		return -ENOMEM;
	}

	/* Init and register Input device */
	input_dev->name = FTS_DRIVER_NAME;
	if (ts_data->bus_type == BUS_TYPE_I2C)
		input_dev->id.bustype = BUS_I2C;
	else
		input_dev->id.bustype = BUS_SPI;
	input_dev->dev.parent = ts_data->dev;

	input_set_drvdata(input_dev, ts_data);

	__set_bit(EV_SYN, input_dev->evbit);
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	input_mt_init_slots(input_dev, pdata->max_touch_number,
			    INPUT_MT_DIRECT);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, pdata->x_min,
			     pdata->x_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, pdata->y_min,
			     pdata->y_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 0xFF, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 0xFF, 0, 0);

	ret = input_register_device(input_dev);
	if (ret) {
		dev_err(ts_data->dev, "Input device registration failed");
		input_set_drvdata(input_dev, NULL);
		input_free_device(input_dev);
		input_dev = NULL;
		return ret;
	}

	ts_data->input_dev = input_dev;

	return 0;
}

static int fts_buffer_init(struct fts_ts_data *ts_data)
{
	ts_data->touch_buf =
		devm_kzalloc(ts_data->dev, FTS_MAX_TOUCH_BUF, GFP_KERNEL);
	if (!ts_data->touch_buf)
		return -ENOMEM;

	ts_data->touch_size = FTS_TOUCH_DATA_LEN;

	return 0;
}

static int fts_power_on(struct fts_ts_data *ts_data)
{
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(fts_tp_supplies),
				    ts_data->supplies);
	if (ret < 0) {
		regulator_bulk_disable(ARRAY_SIZE(fts_tp_supplies),
				       ts_data->supplies);
		return ret;
	}

	gpiod_set_value_cansleep(ts_data->reset_gpio, 0);

	return 0;
}

static void fts_power_off(struct fts_ts_data *ts_data)
{
	gpiod_set_value_cansleep(ts_data->reset_gpio, 1);
	regulator_bulk_disable(ARRAY_SIZE(fts_tp_supplies), ts_data->supplies);
}

static int fts_power_suspend(struct fts_ts_data *ts_data)
{
	fts_power_off(ts_data);

	return 0;
}

static int fts_power_resume(struct fts_ts_data *ts_data)
{
	return fts_power_on(ts_data);
}

static int fts_gpio_configure(struct fts_ts_data *data)
{
	int ret = 0;

	/* request irq gpio */
	if (gpio_is_valid(data->pdata->irq_gpio)) {
		ret = gpio_request(data->pdata->irq_gpio, "fts_irq_gpio");
		if (ret) {
			dev_err(data->dev, "[GPIO]irq gpio request failed");
			goto err_irq_gpio_req;
		}

		ret = gpio_direction_input(data->pdata->irq_gpio);
		if (ret) {
			dev_err(data->dev,
				"[GPIO]set_direction for irq gpio failed");
			goto err_irq_gpio_dir;
		}
	}

	return 0;

err_irq_gpio_dir:
	if (gpio_is_valid(data->pdata->irq_gpio))
		gpio_free(data->pdata->irq_gpio);
err_irq_gpio_req:
	return ret;
}

static int fts_get_dt_coords(struct device *dev, char *name,
			     struct fts_ts_platform_data *pdata)
{
	int ret = 0;
	u32 coords[FTS_COORDS_ARR_SIZE] = { 0 };
	struct property *prop;
	struct device_node *np = dev->of_node;
	int coords_size;

	prop = of_find_property(np, name, NULL);
	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;

	coords_size = prop->length / sizeof(u32);
	if (coords_size != FTS_COORDS_ARR_SIZE) {
		dev_err(dev, "invalid:%s, size:%d", name, coords_size);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, name, coords, coords_size);
	if (ret < 0) {
		dev_err(dev, "Unable to read %s, please check dts", name);
		pdata->x_min = FTS_X_MIN_DISPLAY_DEFAULT;
		pdata->y_min = FTS_Y_MIN_DISPLAY_DEFAULT;
		pdata->x_max = FTS_X_MAX_DISPLAY_DEFAULT;
		pdata->y_max = FTS_Y_MAX_DISPLAY_DEFAULT;
		return -ENODATA;
	} else {
		pdata->x_min = coords[0];
		pdata->y_min = coords[1];
		pdata->x_max = coords[2];
		pdata->y_max = coords[3];
	}

	dev_info(dev, "display x(%d %d) y(%d %d)", pdata->x_min, pdata->x_max,
		 pdata->y_min, pdata->y_max);
	return 0;
}

static int fts_parse_dt(struct device *dev, struct fts_ts_platform_data *pdata)
{
	int ret = 0;
	struct device_node *np = dev->of_node;
	u32 temp_val = 0;

	ret = fts_get_dt_coords(dev, "focaltech,display-coords", pdata);
	if (ret < 0)
		dev_err(dev, "Unable to get display-coords");

	pdata->irq_gpio = of_get_named_gpio(np, "focaltech,irq-gpio", 0);
	if (pdata->irq_gpio < 0)
		dev_err(dev, "Unable to get irq_gpio");

	ret = of_property_read_u32(np, "focaltech,max-touch-number", &temp_val);
	if (ret < 0) {
		dev_err(dev,
			"Unable to get max-touch-number, please check dts");
		pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
	} else {
		if (temp_val < 2)
			/* max_touch_number must >= 2 */
			pdata->max_touch_number = 2;
		else if (temp_val > FTS_MAX_POINTS_SUPPORT)
			pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
		else
			pdata->max_touch_number = temp_val;
	}

	dev_info(dev, "max touch number:%d, irq gpio:%d",
		 pdata->max_touch_number, pdata->irq_gpio);

	return 0;
}

static void fts_resume_work(struct work_struct *work)
{
	struct fts_ts_data *ts_data =
		container_of(work, struct fts_ts_data, resume_work);

	fts_ts_resume(ts_data->dev);
}

static int fts_ts_suspend(struct device *dev)
{
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;

	if (ts_data->suspended) {
		dev_info(dev, "Already in suspend state");
		return 0;
	}

	if (ts_data->fw_loading) {
		dev_info(dev, "fw upgrade in process, can't suspend");
		return 0;
	}

	dev_info(dev, "make TP enter into sleep mode");
	ret = fts_write_reg(FTS_REG_POWER_MODE, FTS_REG_POWER_MODE_SLEEP);
	if (ret < 0)
		dev_err(dev, "set TP to sleep mode fail, ret=%d", ret);

	if (!ts_data->ic_info.is_incell) {
		ret = fts_power_suspend(ts_data);
		if (ret < 0)
			dev_err(dev, "power enter suspend fail");
	}

	fts_release_all_finger();
	ts_data->suspended = true;

	return 0;
}

static int fts_ts_resume(struct device *dev)
{
	struct fts_ts_data *ts_data = fts_data;
	struct input_dev *input_dev = ts_data->input_dev;

	dev_info(dev, "blank flag = %d", ts_data->blank_up);
	if ((ts_data->fod_info.fp_down_report) && (ts_data->blank_up)) {
		ts_data->fod_info.fp_down_report = 0;
		input_sync(input_dev);
	}

	if (!ts_data->suspended) {
		dev_dbg(dev, "Already in awake state");
		return 0;
	}

	ts_data->suspended = false;
	fts_release_all_finger();

	if (!ts_data->ic_info.is_incell)
		fts_power_resume(ts_data);

	fts_wait_tp_to_valid();

	ts_data->blank_up = 0;
	return 0;
}

static int fts_ts_probe(struct spi_device *spi)
{
	struct fts_ts_data *ts_data = NULL;
	int ret = 0;

	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;
	ret = spi_setup(spi);
	if (ret)
		return ret;

	ts_data = devm_kzalloc(&spi->dev, sizeof(*ts_data), GFP_KERNEL);
	if (!ts_data)
		return -ENOMEM;

	ts_data->pdata = devm_kzalloc(
		&spi->dev, sizeof(struct fts_ts_platform_data), GFP_KERNEL);
	if (!ts_data->pdata)
		return -ENOMEM;

	/* Get reset GPIO */
	ts_data->reset_gpio =
		devm_gpiod_get_optional(&spi->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ts_data->reset_gpio))
		return dev_err_probe(&spi->dev, PTR_ERR(ts_data->reset_gpio),
				     "Failed to get reset gpio\n");

	/* Get regulators */
	ret = devm_regulator_bulk_get_const(&spi->dev,
					    ARRAY_SIZE(fts_tp_supplies),
					    fts_tp_supplies,
					    &ts_data->supplies);
	if (ret < 0)
		return dev_err_probe(&spi->dev, ret,
				     "Failed to get regulators\n");

	/* Power ON */
	ret = fts_power_on(ts_data);
	if (ret)
		return dev_err_probe(&spi->dev, ret, "Failed power on\n");

	/* Get firmware path */
	ret = of_property_read_string(spi->dev.of_node, "firmware-name", &ts_data->firmware_path);
	if (ret)
		return dev_err_probe(&spi->dev, ret,
				     "Failed to read firmware-name property\n");

	fts_data = ts_data;
	ts_data->spi = spi;
	ts_data->dev = &spi->dev;

	ts_data->bus_type = BUS_TYPE_SPI_V2;
	spi_set_drvdata(spi, ts_data);

	if (ts_data->dev->of_node) {
		ret = fts_parse_dt(ts_data->dev, ts_data->pdata);
		if (ret)
			dev_err(&spi->dev, "device-tree parse fail");
	} else {
		if (ts_data->dev->platform_data)
			memcpy(ts_data->pdata, ts_data->dev->platform_data,
			       sizeof(struct fts_ts_platform_data));
		else
			return dev_err_probe(&spi->dev, -ENODEV,
					     "Platform_data is null\n");
	}

	ts_data->ts_workqueue = create_singlethread_workqueue("fts_wq");
	if (!ts_data->ts_workqueue)
		dev_err(&spi->dev, "create fts workqueue fail");

	spin_lock_init(&ts_data->irq_lock);
	mutex_init(&ts_data->report_mutex);
	mutex_init(&ts_data->bus_lock);
	init_waitqueue_head(&ts_data->ts_waitqueue);

	/* Init communication interface */
	ret = fts_bus_init(ts_data);
	if (ret) {
		dev_err(&spi->dev, "bus initialize fail");
		goto err_bus_init;
	}

	ret = fts_input_init(ts_data);
	if (ret) {
		dev_err(&spi->dev, "input initialize fail");
		goto err_input_init;
	}

	ret = fts_buffer_init(ts_data);
	if (ret) {
		dev_err(&spi->dev, "buffer init fail");
		goto err_buffer_init;
	}

	ret = fts_gpio_configure(ts_data);
	if (ret)
		dev_err(&spi->dev, "configure the gpios fail");

#if (!FTS_CHIP_IDC)
	fts_request_handle_reset(ts_data, 200);
#endif

	ret = fts_get_ic_information(ts_data);
	if (ret) {
		dev_err(&spi->dev, "not focal IC, unregister driver");
		goto err_irq_req;
	}

	ret = fts_irq_registration(ts_data);
	if (ret) {
		dev_err(&spi->dev, "request irq failed");
		goto err_irq_req;
	}

	ret = fts_fwupg_init(ts_data);
	if (ret)
		dev_err(&spi->dev, "init fw upgrade fail");

	if (ts_data->ts_workqueue)
		INIT_WORK(&ts_data->resume_work, fts_resume_work);

	device_init_wakeup(ts_data->dev, true);

	dev_dbg(&spi->dev, "FocalTech Touchscreen Controller");

	return 0;

err_irq_req:
	if (gpio_is_valid(ts_data->pdata->irq_gpio))
		gpio_free(ts_data->pdata->irq_gpio);
err_buffer_init:
	input_unregister_device(ts_data->input_dev);
err_input_init:
	if (ts_data->ts_workqueue)
		destroy_workqueue(ts_data->ts_workqueue);
err_bus_init:
	kfree_safe(ts_data->bus_tx_buf);
	kfree_safe(ts_data->bus_rx_buf);

	return ret;
}

static void fts_ts_remove(struct spi_device *spi)
{
	cancel_work_sync(&fts_data->resume_work);
	fts_fwupg_exit(fts_data);
	free_irq(fts_data->irq, fts_data);
	fts_bus_exit(fts_data);
	input_unregister_device(fts_data->input_dev);

	if (fts_data->ts_workqueue)
		destroy_workqueue(fts_data->ts_workqueue);

	if (gpio_is_valid(fts_data->pdata->irq_gpio))
		gpio_free(fts_data->pdata->irq_gpio);

	fts_power_off(fts_data);
}

static const struct spi_device_id fts_ts_id[] = {
	{ "ft3680" },
	{ },
};
MODULE_DEVICE_TABLE(spi, fts_ts_id);

static const struct of_device_id fts_dt_match[] = {
	{ .compatible = "focaltech,ft3680", },
	{ },
};
MODULE_DEVICE_TABLE(of, fts_dt_match);

static struct spi_driver fts_ts_driver = {
	.probe = fts_ts_probe,
	.remove = fts_ts_remove,
	.driver = {
		.name = FTS_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(fts_dt_match),
	},
	.id_table = fts_ts_id,
};
module_spi_driver(fts_ts_driver);

MODULE_AUTHOR("FocalTech Driver Team");
MODULE_DESCRIPTION("FocalTech Touchscreen Driver");
MODULE_LICENSE("GPL v2");
