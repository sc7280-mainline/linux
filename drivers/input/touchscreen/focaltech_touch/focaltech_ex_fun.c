/*
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, Focaltech Ltd. All rights reserved.
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

#include "focaltech_core.h"

enum {
	RWREG_OP_READ = 0,
	RWREG_OP_WRITE = 1,
};

static struct rwreg_operation_t {
	int type;	/*  0: read, 1: write */
	int reg;	/*  register */
	int len;	/*  read/write length */
	int val;	/*  length = 1; read: return value, write: op return */
	int res;	/*  0: success, otherwise: fail */
	char *opbuf;	/*  length >= 1, read return value, write: op return */
} rw_op;

static int fts_ta_open(struct inode *inode, struct file *file)
{
	struct fts_ts_data *ts_data = pde_data(inode);

	if (ts_data->touch_analysis_support) {
		dev_info(ts_data->dev, "fts_ta open");
		ts_data->ta_buf = kzalloc(FTS_MAX_TOUCH_BUF, GFP_KERNEL);
		if (!ts_data->ta_buf) {
			dev_err(ts_data->dev, "kzalloc for ta_buf fails");
			return -ENOMEM;
		}
	}
	return 0;
}

static int fts_ta_release(struct inode *inode, struct file *file)
{
	struct fts_ts_data *ts_data = pde_data(inode);

	if (ts_data->touch_analysis_support) {
		dev_info(ts_data->dev, "fts_ta close");
		ts_data->ta_flag = 0;
		if (ts_data->ta_buf) {
			kfree(ts_data->ta_buf);
			ts_data->ta_buf = NULL;
		}
	}
	return 0;
}

static ssize_t fts_ta_read(struct file *filp, char __user *buff, size_t count,
			   loff_t *ppos)
{
	int read_num = (int)count;
	struct fts_ts_data *ts_data = pde_data(file_inode(filp));

	if (!ts_data->touch_analysis_support || !ts_data->ta_buf) {
		dev_err(ts_data->dev,
			"touch_analysis is disabled, or ta_buf is NULL");
		return -EINVAL;
	}

	if (!(filp->f_flags & O_NONBLOCK)) {
		ts_data->ta_flag = 1;
		wait_event_interruptible(ts_data->ts_waitqueue,
					 !ts_data->ta_flag);
	}

	read_num = (ts_data->ta_size < read_num) ? ts_data->ta_size : read_num;
	if ((read_num > 0) && (copy_to_user(buff, ts_data->ta_buf, read_num))) {
		dev_err(ts_data->dev, "copy to user error");
		return -EFAULT;
	}

	return read_num;
}

static const struct proc_ops fts_procta_fops = {
	.proc_open = fts_ta_open,
	.proc_release = fts_ta_release,
	.proc_read = fts_ta_read,
};

/************************************************************************
 * sysfs interface
 ***********************************************************************/
/* fts_hw_reset interface */
static ssize_t fts_hw_reset_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct input_dev *input_dev = ts_data->input_dev;
	ssize_t count = 0;

	mutex_lock(&input_dev->mutex);
	fts_request_handle_reset(ts_data, 0);
	count = snprintf(buf, PAGE_SIZE, "hw reset executed\n");
	mutex_unlock(&input_dev->mutex);

	return count;
}

static ssize_t fts_hw_reset_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	return -EPERM;
}

/* fts_irq interface */
static ssize_t fts_irq_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	ssize_t count = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct irq_desc *desc =
		irq_data_to_desc(irq_get_irq_data(ts_data->irq));

	count = snprintf(buf, PAGE_SIZE, "irq_depth:%d\n", desc->depth);

	return count;
}

static ssize_t fts_irq_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct input_dev *input_dev = ts_data->input_dev;

	mutex_lock(&input_dev->mutex);
	if (FTS_SYSFS_ECHO_ON(buf)) {
		dev_info(ts_data->dev, "enable irq");
		fts_irq_enable();
	} else if (FTS_SYSFS_ECHO_OFF(buf)) {
		dev_info(ts_data->dev, "disable irq");
		fts_irq_disable();
	}
	mutex_unlock(&input_dev->mutex);
	return count;
}

/* fts_boot_mode interface */
static ssize_t fts_bootmode_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct input_dev *input_dev = ts_data->input_dev;

	mutex_lock(&input_dev->mutex);
	if (FTS_SYSFS_ECHO_ON(buf)) {
		dev_info(ts_data->dev, "[EX-FUN]set to boot mode");
		ts_data->fw_is_running = false;
	} else if (FTS_SYSFS_ECHO_OFF(buf)) {
		dev_info(ts_data->dev, "[EX-FUN]set to fw mode");
		ts_data->fw_is_running = true;
	}
	mutex_unlock(&input_dev->mutex);

	return count;
}

static ssize_t fts_bootmode_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct input_dev *input_dev = ts_data->input_dev;

	mutex_lock(&input_dev->mutex);
	if (true == ts_data->fw_is_running) {
		count = snprintf(buf, PAGE_SIZE, "tp is in fw mode\n");
	} else {
		count = snprintf(buf, PAGE_SIZE, "tp is in boot mode\n");
	}
	mutex_unlock(&input_dev->mutex);

	return count;
}

/* fts_tpfwver interface */
static ssize_t fts_tpfwver_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct input_dev *input_dev = ts_data->input_dev;
	ssize_t num_read_chars = 0;
	u8 fwver = 0;

	mutex_lock(&input_dev->mutex);

	ret = fts_read_reg(FTS_REG_FW_VER, &fwver);
	if ((ret < 0) || (fwver == 0xFF) || (fwver == 0x00))
		num_read_chars =
			snprintf(buf, PAGE_SIZE, "get tp fw version fail!\n");
	else
		num_read_chars = snprintf(buf, PAGE_SIZE, "%02x\n", fwver);

	mutex_unlock(&input_dev->mutex);
	return num_read_chars;
}

static ssize_t fts_tpfwver_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	return -EPERM;
}

/* fts_rw_reg */
static ssize_t fts_tprwreg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int count;
	int i;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct input_dev *input_dev = ts_data->input_dev;

	mutex_lock(&input_dev->mutex);

	if (rw_op.len < 0) {
		count = snprintf(buf, PAGE_SIZE, "Invalid cmd line\n");
	} else if (rw_op.len == 1) {
		if (RWREG_OP_READ == rw_op.type) {
			if (rw_op.res == 0) {
				count = snprintf(buf, PAGE_SIZE,
						 "Read %02X: %02X\n", rw_op.reg,
						 rw_op.val);
			} else {
				count = snprintf(buf, PAGE_SIZE,
						 "Read %02X failed, ret: %d\n",
						 rw_op.reg, rw_op.res);
			}
		} else {
			if (rw_op.res == 0) {
				count = snprintf(buf, PAGE_SIZE,
						 "Write %02X, %02X success\n",
						 rw_op.reg, rw_op.val);
			} else {
				count = snprintf(buf, PAGE_SIZE,
						 "Write %02X failed, ret: %d\n",
						 rw_op.reg, rw_op.res);
			}
		}
	} else {
		if (RWREG_OP_READ == rw_op.type) {
			count = snprintf(buf, PAGE_SIZE,
					 "Read Reg: [%02X]-[%02X]\n", rw_op.reg,
					 rw_op.reg + rw_op.len);
			count += snprintf(buf + count, PAGE_SIZE, "Result: ");
			if (rw_op.res) {
				count += snprintf(buf + count, PAGE_SIZE,
						  "failed, ret: %d\n",
						  rw_op.res);
			} else {
				if (rw_op.opbuf) {
					for (i = 0; i < rw_op.len; i++) {
						count += snprintf(
							buf + count, PAGE_SIZE,
							"%02X ",
							rw_op.opbuf[i]);
					}
					count += snprintf(buf + count,
							  PAGE_SIZE, "\n");
				}
			}
		} else {
			;
			count = snprintf(buf, PAGE_SIZE,
					 "Write Reg: [%02X]-[%02X]\n",
					 rw_op.reg, rw_op.reg + rw_op.len - 1);
			count += snprintf(buf + count, PAGE_SIZE,
					  "Write Data: ");
			if (rw_op.opbuf) {
				for (i = 1; i < rw_op.len; i++) {
					count += snprintf(buf + count,
							  PAGE_SIZE, "%02X ",
							  rw_op.opbuf[i]);
				}
				count += snprintf(buf + count, PAGE_SIZE, "\n");
			}
			if (rw_op.res) {
				count += snprintf(buf + count, PAGE_SIZE,
						  "Result: failed, ret: %d\n",
						  rw_op.res);
			} else {
				count += snprintf(buf + count, PAGE_SIZE,
						  "Result: success\n");
			}
		}
		/*if (rw_op.opbuf) {
            kfree(rw_op.opbuf);
            rw_op.opbuf = NULL;
        }*/
	}
	mutex_unlock(&input_dev->mutex);

	return count;
}

static int shex_to_int(const char *hex_buf, int size)
{
	int i;
	int base = 1;
	int value = 0;
	char single;

	for (i = size - 1; i >= 0; i--) {
		single = hex_buf[i];

		if ((single >= '0') && (single <= '9')) {
			value += (single - '0') * base;
		} else if ((single >= 'a') && (single <= 'z')) {
			value += (single - 'a' + 10) * base;
		} else if ((single >= 'A') && (single <= 'Z')) {
			value += (single - 'A' + 10) * base;
		} else {
			return -EINVAL;
		}

		base *= 16;
	}

	return value;
}

static u8 shex_to_u8(const char *hex_buf, int size)
{
	return (u8)shex_to_int(hex_buf, size);
}
/*
 * Format buf:
 * [0]: '0' write, '1' read(reserved)
 * [1-2]: addr, hex
 * [3-4]: length, hex
 * [5-6]...[n-(n+1)]: data, hex
 */
static int fts_parse_buf(const char *buf, size_t cmd_len)
{
	int length;
	int i;
	char *tmpbuf;

	rw_op.reg = shex_to_u8(buf + 1, 2);
	length = shex_to_int(buf + 3, 2);

	if (buf[0] == '1') {
		rw_op.len = length;
		rw_op.type = RWREG_OP_READ;
		dev_dbg(fts_data->dev, "read %02X, %d bytes", rw_op.reg,
			rw_op.len);
	} else {
		if (cmd_len < (length * 2 + 5)) {
			pr_err("data invalided!\n");
			return -EINVAL;
		}
		dev_dbg(fts_data->dev, "write %02X, %d bytes", rw_op.reg,
			length);

		/* first byte is the register addr */
		rw_op.type = RWREG_OP_WRITE;
		rw_op.len = length + 1;
	}

	if (rw_op.len > 0) {
		tmpbuf = (char *)kzalloc(rw_op.len, GFP_KERNEL);
		if (!tmpbuf) {
			dev_err(fts_data->dev, "allocate memory failed!\n");
			return -ENOMEM;
		}

		if (RWREG_OP_WRITE == rw_op.type) {
			tmpbuf[0] = rw_op.reg & 0xFF;
			dev_dbg(fts_data->dev, "write buffer: ");
			for (i = 1; i < rw_op.len; i++) {
				tmpbuf[i] = shex_to_u8(buf + 5 + i * 2 - 2, 2);
				dev_dbg(fts_data->dev, "buf[%d]: %02X", i,
					tmpbuf[i] & 0xFF);
			}
		}
		rw_op.opbuf = tmpbuf;
	}

	return rw_op.len;
}

static ssize_t fts_tprwreg_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct input_dev *input_dev = ts_data->input_dev;
	ssize_t cmd_length = 0;

	mutex_lock(&input_dev->mutex);
	cmd_length = count - 1;

	if (rw_op.opbuf) {
		kfree(rw_op.opbuf);
		rw_op.opbuf = NULL;
	}

	dev_dbg(fts_data->dev, "cmd len: %d, buf: %s", (int)cmd_length, buf);
	/* compatible old ops */
	if (2 == cmd_length) {
		rw_op.type = RWREG_OP_READ;
		rw_op.len = 1;
		rw_op.reg = shex_to_int(buf, 2);
	} else if (4 == cmd_length) {
		rw_op.type = RWREG_OP_WRITE;
		rw_op.len = 1;
		rw_op.reg = shex_to_int(buf, 2);
		rw_op.val = shex_to_int(buf + 2, 2);
	} else if (cmd_length < 5) {
		dev_err(fts_data->dev, "Invalid cmd buffer");
		mutex_unlock(&input_dev->mutex);
		return -EINVAL;
	} else {
		rw_op.len = fts_parse_buf(buf, cmd_length);
	}

	if (rw_op.len < 0) {
		dev_err(fts_data->dev, "cmd buffer error!");

	} else {
		if (RWREG_OP_READ == rw_op.type) {
			if (rw_op.len == 1) {
				u8 reg, val;
				reg = rw_op.reg & 0xFF;
				rw_op.res = fts_read_reg(reg, &val);
				rw_op.val = val;
			} else {
				char reg;
				reg = rw_op.reg & 0xFF;

				rw_op.res = fts_read(&reg, 1, rw_op.opbuf,
						     rw_op.len);
			}

			if (rw_op.res < 0) {
				dev_err(fts_data->dev, "Could not read 0x%02x",
					rw_op.reg);
			} else {
				dev_info(fts_data->dev,
					 "read 0x%02x, %d bytes successful",
					 rw_op.reg, rw_op.len);
				rw_op.res = 0;
			}

		} else {
			if (rw_op.len == 1) {
				u8 reg, val;
				reg = rw_op.reg & 0xFF;
				val = rw_op.val & 0xFF;
				rw_op.res = fts_write_reg(reg, val);
			} else {
				rw_op.res = fts_write(rw_op.opbuf, rw_op.len);
			}
			if (rw_op.res < 0) {
				dev_err(fts_data->dev, "Could not write 0x%02x",
					rw_op.reg);

			} else {
				dev_info(fts_data->dev,
					 "Write 0x%02x, %d bytes successful",
					 rw_op.val, rw_op.len);
				rw_op.res = 0;
			}
		}
	}

	mutex_unlock(&input_dev->mutex);
	return count;
}

/* fts_upgrade_bin interface */
static ssize_t fts_fwupgradebin_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return -EPERM;
}

static ssize_t fts_fwupgradebin_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	char fwname[FILE_NAME_LENGTH] = { 0 };
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct input_dev *input_dev = ts_data->input_dev;

	if ((count <= 1) || (count >= FILE_NAME_LENGTH - 32)) {
		dev_err(fts_data->dev, "fw bin name's length(%d) fail",
			(int)count);
		return -EINVAL;
	}
	memset(fwname, 0, sizeof(fwname));
	snprintf(fwname, FILE_NAME_LENGTH, "%s", buf);
	fwname[count - 1] = '\0';

	dev_info(fts_data->dev, "upgrade with bin file through sysfs node");
	mutex_lock(&input_dev->mutex);
	fts_upgrade_bin(fwname, 0);
	mutex_unlock(&input_dev->mutex);

	return count;
}

/* fts_force_upgrade interface */
static ssize_t fts_fwforceupg_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return -EPERM;
}

static ssize_t fts_fwforceupg_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	char fwname[FILE_NAME_LENGTH];
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct input_dev *input_dev = ts_data->input_dev;

	if ((count <= 1) || (count >= FILE_NAME_LENGTH - 32)) {
		dev_err(fts_data->dev, "fw bin name's length(%d) fail",
			(int)count);
		return -EINVAL;
	}
	memset(fwname, 0, sizeof(fwname));
	snprintf(fwname, FILE_NAME_LENGTH, "%s", buf);
	fwname[count - 1] = '\0';

	dev_info(fts_data->dev, "force upgrade through sysfs node");
	mutex_lock(&input_dev->mutex);
	fts_upgrade_bin(fwname, 1);
	mutex_unlock(&input_dev->mutex);

	return count;
}

/* fts_driver_info interface */
static ssize_t fts_driverinfo_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int count = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct fts_ts_platform_data *pdata = ts_data->pdata;
	struct input_dev *input_dev = ts_data->input_dev;

	mutex_lock(&input_dev->mutex);
	count += snprintf(buf + count, PAGE_SIZE,
			  "Resolution:(%d,%d)~(%d,%d)\n", pdata->x_min,
			  pdata->y_min, pdata->x_max, pdata->y_max);

	count += snprintf(buf + count, PAGE_SIZE, "Max Touchs:%d\n",
			  pdata->max_touch_number);

	count += snprintf(buf + count, PAGE_SIZE, "int gpio:%d,irq:%d\n",
			  pdata->irq_gpio, ts_data->irq);

	count += snprintf(buf + count, PAGE_SIZE, "IC ID:0x%02x%02x\n",
			  ts_data->ic_info.ids.chip_idh,
			  ts_data->ic_info.ids.chip_idl);

	if (ts_data->bus_type == BUS_TYPE_I2C) {
		count += snprintf(buf + count, PAGE_SIZE, "BUS:%s,addr:0x%x\n",
				  "I2C", ts_data->client->addr);
	} else {
		count += snprintf(buf + count, PAGE_SIZE,
				  "BUS:%s,mode:%d,max_freq:%d\n", "SPI",
				  ts_data->spi->mode,
				  ts_data->spi->max_speed_hz);
	}
	mutex_unlock(&input_dev->mutex);

	return count;
}

static ssize_t fts_driverinfo_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	return -EPERM;
}

/* fts_dump_reg interface */
static ssize_t fts_dumpreg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int count = 0;
	u8 val = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct input_dev *input_dev = ts_data->input_dev;

	mutex_lock(&input_dev->mutex);

	fts_read_reg(FTS_REG_POWER_MODE, &val);
	count += snprintf(buf + count, PAGE_SIZE, "Power Mode:0x%02x\n", val);

	fts_read_reg(FTS_REG_FW_VER, &val);
	count += snprintf(buf + count, PAGE_SIZE, "FW Ver:0x%02x\n", val);

	fts_read_reg(FTS_REG_LIC_VER, &val);
	count += snprintf(buf + count, PAGE_SIZE, "LCD Initcode Ver:0x%02x\n",
			  val);

	fts_read_reg(FTS_REG_IDE_PARA_VER_ID, &val);
	count += snprintf(buf + count, PAGE_SIZE, "Param Ver:0x%02x\n", val);

	fts_read_reg(FTS_REG_IDE_PARA_STATUS, &val);
	count += snprintf(buf + count, PAGE_SIZE, "Param status:0x%02x\n", val);

	fts_read_reg(FTS_REG_VENDOR_ID, &val);
	count += snprintf(buf + count, PAGE_SIZE, "Vendor ID:0x%02x\n", val);

	fts_read_reg(FTS_REG_CHARGER_MODE_EN, &val);
	count += snprintf(buf + count, PAGE_SIZE, "charge stat:0x%02x\n", val);

	fts_read_reg(FTS_REG_INT_CNT, &val);
	count += snprintf(buf + count, PAGE_SIZE, "INT count:0x%02x\n", val);

	fts_read_reg(FTS_REG_FLOW_WORK_CNT, &val);
	count += snprintf(buf + count, PAGE_SIZE, "ESD count:0x%02x\n", val);

	fts_read_reg(FTS_REG_EDGE_MODE_EN, &val);
	count += snprintf(buf + count, PAGE_SIZE, "edge mode stat:0x%02x\n",
			  val);

	mutex_unlock(&input_dev->mutex);
	return count;
}

static ssize_t fts_dumpreg_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	return -EPERM;
}

/* fts_dump_reg interface */
static ssize_t fts_tpbuf_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	int count = 0;
	int i = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct input_dev *input_dev = ts_data->input_dev;

	mutex_lock(&input_dev->mutex);
	count += snprintf(buf + count, PAGE_SIZE, "touch point buffer:\n");
	for (i = 0; i < FTS_TOUCH_DATA_LEN; i++) {
		count += snprintf(buf + count, PAGE_SIZE, "%02x ",
				  ts_data->touch_buf[i]);
	}
	count += snprintf(buf + count, PAGE_SIZE, "\n");
	mutex_unlock(&input_dev->mutex);

	return count;
}

static ssize_t fts_tpbuf_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	return -EPERM;
}

/* fts_log_level node */
static ssize_t fts_log_level_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int count = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct input_dev *input_dev = ts_data->input_dev;

	mutex_lock(&input_dev->mutex);
	count += snprintf(buf + count, PAGE_SIZE, "log level:%d\n",
			  ts_data->log_level);
	mutex_unlock(&input_dev->mutex);

	return count;
}

static ssize_t fts_log_level_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int value = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct input_dev *input_dev = ts_data->input_dev;

	mutex_lock(&input_dev->mutex);
	sscanf(buf, "%d", &value);
	dev_dbg(fts_data->dev, "log level:%d->%d", ts_data->log_level, value);
	ts_data->log_level = value;
	mutex_unlock(&input_dev->mutex);

	return count;
}

/* fts_touch_size node */
static ssize_t fts_touchsize_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int count = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct input_dev *input_dev = ts_data->input_dev;

	mutex_lock(&input_dev->mutex);
	count += snprintf(buf + count, PAGE_SIZE, "touch size:%d\n",
			  ts_data->touch_size);
	mutex_unlock(&input_dev->mutex);

	return count;
}

static ssize_t fts_touchsize_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int value = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct input_dev *input_dev = ts_data->input_dev;

	mutex_lock(&input_dev->mutex);
	sscanf(buf, "%d", &value);
	if ((value > 2) && (value < FTS_MAX_TOUCH_BUF)) {
		dev_dbg(fts_data->dev, "touch size:%d->%d", ts_data->touch_size,
			value);
		ts_data->touch_size = value;
	} else {
		dev_dbg(fts_data->dev, "touch size:%d invalid", value);
	}
	mutex_unlock(&input_dev->mutex);

	return count;
}

/* fts_ta_mode node */
static ssize_t fts_tamode_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	int count = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct input_dev *input_dev = ts_data->input_dev;

	mutex_lock(&input_dev->mutex);
	count += snprintf(buf + count, PAGE_SIZE, "touch analysis:%s\n",
			  ts_data->touch_analysis_support ? "Enable" :
							    "Disable");
	mutex_unlock(&input_dev->mutex);

	return count;
}

static ssize_t fts_coordinate_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int count = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	count = snprintf(buf, PAGE_SIZE, "x:%02x\ny:%02x\n", ts_data->xbuf,
			 ts_data->ybuf);

	return count;
}

static ssize_t fts_tamode_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	int value = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct input_dev *input_dev = ts_data->input_dev;

	mutex_lock(&input_dev->mutex);
	sscanf(buf, "%d", &value);
	ts_data->touch_analysis_support = !!value;
	dev_dbg(fts_data->dev, "set touch analysis:%d",
		ts_data->touch_analysis_support);
	mutex_unlock(&input_dev->mutex);

	return count;
}

/* get the fw version  example:cat fw_version */
static DEVICE_ATTR(fts_fw_version, S_IRUGO | S_IWUSR, fts_tpfwver_show,
		   fts_tpfwver_store);

/* read and write register(s)
*   All data type is **HEX**
*   Single Byte:
*       read:   echo 88 > rw_reg ---read register 0x88
*       write:  echo 8807 > rw_reg ---write 0x07 into register 0x88
*   Multi-bytes:
*       [0:rw-flag][1-2: reg addr, hex][3-4: length, hex][5-6...n-n+1: write data, hex]
*       rw-flag: 0, write; 1, read
*       read:  echo 10005           > rw_reg ---read reg 0x00-0x05
*       write: echo 000050102030405 > rw_reg ---write reg 0x00-0x05 as 01,02,03,04,05
*  Get result:
*       cat rw_reg
*/
static DEVICE_ATTR(fts_rw_reg, S_IRUGO | S_IWUSR, fts_tprwreg_show,
		   fts_tprwreg_store);
/*  upgrade from fw bin file   example:echo "*.bin" > fts_upgrade_bin */
static DEVICE_ATTR(fts_upgrade_bin, S_IRUGO | S_IWUSR, fts_fwupgradebin_show,
		   fts_fwupgradebin_store);
static DEVICE_ATTR(fts_force_upgrade, S_IRUGO | S_IWUSR, fts_fwforceupg_show,
		   fts_fwforceupg_store);
static DEVICE_ATTR(fts_driver_info, S_IRUGO | S_IWUSR, fts_driverinfo_show,
		   fts_driverinfo_store);
static DEVICE_ATTR(fts_dump_reg, S_IRUGO | S_IWUSR, fts_dumpreg_show,
		   fts_dumpreg_store);
static DEVICE_ATTR(fts_hw_reset, S_IRUGO | S_IWUSR, fts_hw_reset_show,
		   fts_hw_reset_store);
static DEVICE_ATTR(fts_irq, S_IRUGO | S_IWUSR, fts_irq_show, fts_irq_store);
static DEVICE_ATTR(fts_boot_mode, S_IRUGO | S_IWUSR, fts_bootmode_show,
		   fts_bootmode_store);
static DEVICE_ATTR(fts_touch_point, S_IRUGO | S_IWUSR, fts_tpbuf_show,
		   fts_tpbuf_store);
static DEVICE_ATTR(fts_log_level, S_IRUGO | S_IWUSR, fts_log_level_show,
		   fts_log_level_store);
static DEVICE_ATTR(fts_touch_size, S_IRUGO | S_IWUSR, fts_touchsize_show,
		   fts_touchsize_store);
static DEVICE_ATTR(fts_ta_mode, S_IRUGO | S_IWUSR, fts_tamode_show,
		   fts_tamode_store);
static DEVICE_ATTR(fts_parse_coordinate, S_IRUGO, fts_coordinate_show, NULL);

/* add your attr in here*/
static struct attribute *fts_attributes[] = {
	&dev_attr_fts_fw_version.attr,
	&dev_attr_fts_rw_reg.attr,
	&dev_attr_fts_dump_reg.attr,
	&dev_attr_fts_upgrade_bin.attr,
	&dev_attr_fts_force_upgrade.attr,
	&dev_attr_fts_driver_info.attr,
	&dev_attr_fts_hw_reset.attr,
	&dev_attr_fts_irq.attr,
	&dev_attr_fts_boot_mode.attr,
	&dev_attr_fts_touch_point.attr,
	&dev_attr_fts_log_level.attr,
	&dev_attr_fts_touch_size.attr,
	&dev_attr_fts_ta_mode.attr,
	&dev_attr_fts_parse_coordinate.attr,
	NULL
};

static struct attribute_group fts_attribute_group = { .attrs = fts_attributes };

int fts_create_sysfs(struct fts_ts_data *ts_data)
{
	int ret = 0;

	ret = sysfs_create_group(&ts_data->dev->kobj, &fts_attribute_group);
	if (ret) {
		dev_err(fts_data->dev, "[EX]: sysfs_create_group() failed!!");
		sysfs_remove_group(&ts_data->dev->kobj, &fts_attribute_group);
		return -ENOMEM;
	} else {
		dev_info(fts_data->dev,
			 "[EX]: sysfs_create_group() succeeded!!");
	}

	return ret;
}

int fts_remove_sysfs(struct fts_ts_data *ts_data)
{
	sysfs_remove_group(&ts_data->dev->kobj, &fts_attribute_group);
	return 0;
}
