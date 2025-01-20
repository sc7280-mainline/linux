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

#ifndef __LINUX_FOCALTECH_CORE_H__
#define __LINUX_FOCALTECH_CORE_H__

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/dma-mapping.h>

#define FTS_CHIP_TYPE 0x3680008A

#define BYTE_OFF_0(x)		(u8)((x) & 0xFF)
#define BYTE_OFF_8(x)		(u8)(((x) >> 8) & 0xFF)
#define BYTE_OFF_16(x)		(u8)(((x) >> 16) & 0xFF)
#define BYTE_OFF_24(x)		(u8)(((x) >> 24) & 0xFF)
#define FLAGBIT(x)		(0x00000001 << (x))
#define FLAGBITS(x, y)		((0xFFFFFFFF >> (32 - (y) - 1)) & (0xFFFFFFFF << (x)))

#define FLAG_ICSERIALS_LEN	8
#define FLAG_HID_BIT		10
#define FLAG_IDC_BIT		11

#define IC_SERIALS		(FTS_CHIP_TYPE & FLAGBITS(0, FLAG_ICSERIALS_LEN-1))
#define IC_TO_SERIALS(x)	((x) & FLAGBITS(0, FLAG_ICSERIALS_LEN-1))
#define FTS_CHIP_IDC		((FTS_CHIP_TYPE & FLAGBIT(FLAG_IDC_BIT)) == FLAGBIT(FLAG_IDC_BIT))
#define FTS_HID_SUPPORTTED	((FTS_CHIP_TYPE & FLAGBIT(FLAG_HID_BIT)) == FLAGBIT(FLAG_HID_BIT))

#define FTS_MAX_CHIP_IDS	8

#define FTS_CHIP_TYPE_MAPPING	{ { 0x8A, 0x56, 0x62, 0x56, 0x62, 0x56, 0xE2, 0x00, 0x00 } }

#define FILE_NAME_LENGTH			128
#define ENABLE					1
#define DISABLE					0
#define VALID					1
#define INVALID					0
#define FTS_CMD_START1				0x55
#define FTS_CMD_START2				0xAA
#define FTS_CMD_START_DELAY			12
#define FTS_CMD_READ_ID				0x90
#define FTS_CMD_READ_ID_LEN			4
#define FTS_CMD_READ_ID_LEN_INCELL		1
#define FTS_CMD_READ_INFO			0x5E

/*register address*/
#define FTS_REG_INT_CNT				0x8F
#define FTS_REG_FLOW_WORK_CNT			0x91
#define FTS_REG_WORKMODE			0x00
#define FTS_REG_WORKMODE_FACTORY_VALUE		0x40
#define FTS_REG_WORKMODE_WORK_VALUE		0x00
#define FTS_REG_CHIP_ID				0xA3
#define FTS_REG_CHIP_ID2			0x9F
#define FTS_REG_POWER_MODE			0xA5
#define FTS_REG_POWER_MODE_SLEEP		0x03
#define FTS_REG_FW_VER				0xA6
#define FTS_REG_VENDOR_ID			0xA8
#define FTS_REG_LCD_BUSY_NUM			0xAB
#define FTS_REG_FACE_DEC_MODE_EN		0xB0
#define FTS_REG_FACTORY_MODE_DETACH_FLAG	0xB4
#define FTS_REG_FACE_DEC_MODE_STATUS		0x01
#define FTS_REG_IDE_PARA_VER_ID			0xB5
#define FTS_REG_IDE_PARA_STATUS			0xB6
#define FTS_REG_LIC_VER				0xE4
#define FACTORY_REG_OPEN_ADDR			0xCF
#define FACTORY_REG_OPEN_ADDR_FOD		0x02
#define FTS_REG_EDGE_MODE_EN			0x8C

#define FTS_MAX_POINTS_SUPPORT		10 /* constant value, can't be changed */
#define FTS_MAX_KEYS			4
#define FTS_KEY_DIM			10
#define FTS_COORDS_ARR_SIZE		4
#define FTS_ONE_TCH_LEN			6
#define FTS_TOUCH_DATA_LEN		(FTS_MAX_POINTS_SUPPORT * FTS_ONE_TCH_LEN + 2)

#define FTS_SIZE_DEFAULT		15

#define FTS_MAX_ID			0x0A
#define FTS_TOUCH_OFF_E_XH		0
#define FTS_TOUCH_OFF_XL		1
#define FTS_TOUCH_OFF_ID_YH		2
#define FTS_TOUCH_OFF_YL		3
#define FTS_TOUCH_OFF_PRE		4
#define FTS_TOUCH_OFF_AREA		5
#define FTS_TOUCH_E_NUM			1
#define FTS_X_MIN_DISPLAY_DEFAULT	0
#define FTS_Y_MIN_DISPLAY_DEFAULT	0
#define FTS_X_MAX_DISPLAY_DEFAULT	720
#define FTS_Y_MAX_DISPLAY_DEFAULT	1280

#define FTS_TOUCH_DOWN			0
#define FTS_TOUCH_UP			1
#define FTS_TOUCH_CONTACT		2
#define EVENT_DOWN(flag)		((FTS_TOUCH_DOWN == flag) || (FTS_TOUCH_CONTACT == flag))
#define EVENT_UP(flag)			(FTS_TOUCH_UP == flag)

#define FTS_MAX_COMPATIBLE_TYPE		4
#define FTS_MAX_COMMMAND_LENGTH		16

#define FTS_MAX_TOUCH_BUF		4096

#define FTS_REG_FOD_INFO		0xE1
#define FTS_REG_FOD_INFO_LEN		9

#define FTS_TIMEOUT_COMERR_PM		700

#define FTS_SYSFS_ECHO_ON(buf)		(buf[0] == '1' || buf[0] == '2')
#define FTS_SYSFS_ECHO_OFF(buf)		(buf[0] == '0')

#define kfree_safe(pbuf)             \
	do {                         \
		if (pbuf) {          \
			kfree(pbuf); \
			pbuf = NULL; \
		}                    \
	} while (0)


struct ftxxxx_proc {
	struct proc_dir_entry *proc_entry;
	u8 opmode;
	u8 cmd_len;
	u8 cmd[FTS_MAX_COMMMAND_LENGTH];
};

struct fts_ts_platform_data {
	u32 irq_gpio;
	u32 irq_gpio_flags;
	u32 key_number;
	u32 keys[FTS_MAX_KEYS];
	u32 key_y_coords[FTS_MAX_KEYS];
	u32 key_x_coords[FTS_MAX_KEYS];
	u32 x_max;
	u32 y_max;
	u32 x_min;
	u32 y_min;
	u32 max_touch_number;
};

struct ts_event {
	int x;		/*x coordinate */
	int y;		/*y coordinate */
	int p;		/* pressure */
	int flag;	/* touch event flag: 0 -- down; 1-- up; 2 -- contact */
	int id;		/*touch ID */
	int area;
};

struct fts_fod_info {
	u8 fp_id;
	u8 event_type;
	u8 fp_area_rate;
	u8 tp_area;
	u16 fp_x;
	u16 fp_y;
	u8 fp_down;
	u8 fp_down_report;
};

struct ft_chip_t {
	u16 type;
	u8 chip_idh;
	u8 chip_idl;
	u8 rom_idh;
	u8 rom_idl;
	u8 pb_idh;
	u8 pb_idl;
	u8 bl_idh;
	u8 bl_idl;
};

struct ft_chip_id_t {
	u16 type;
	u16 chip_ids[FTS_MAX_CHIP_IDS];
};

struct ts_ic_info {
	bool is_incell;
	bool hid_supported;
	struct ft_chip_t ids;
	struct ft_chip_id_t cid;
};

struct fts_ts_data {
	struct i2c_client *client;
	struct spi_device *spi;
	struct device *dev;
	struct input_dev *input_dev;
	struct fts_ts_platform_data *pdata;
	struct ts_ic_info ic_info;
	struct workqueue_struct *ts_workqueue;
	struct work_struct fwupg_work;
	struct delayed_work prc_work;
	struct work_struct resume_work;
	wait_queue_head_t ts_waitqueue;
	struct ftxxxx_proc proc;
	spinlock_t irq_lock;
	struct mutex report_mutex;
	struct mutex bus_lock;
	unsigned long intr_jiffies;
	int irq;
	int fw_is_running;	/* confirm fw is running when using spi:default 0 */
	int dummy_byte;
	bool suspended;
	bool fw_loading;
	bool irq_disabled;
	bool power_disabled;
	bool prc_support;
	bool prc_mode;
	u8 blank_up;

	struct ts_event events[FTS_MAX_POINTS_SUPPORT];	/* multi-touch */
	u8 touch_addr;
	u32 touch_size;
	u8 *touch_buf;
	int touch_event_num;
	int touch_points;
	int key_state;
	int ta_flag;
	u32 ta_size;
	u8 *ta_buf;
	u16 xbuf;
	u16 ybuf;

	u8 *bus_tx_buf;
	u8 *bus_rx_buf;
	int bus_type;
	struct fts_fod_info fod_info;

	struct regulator_bulk_data *supplies;
	struct gpio_desc *reset_gpio;
	const char *firmware_path;
};

enum _FTS_BUS_TYPE {
	BUS_TYPE_NONE,
	BUS_TYPE_I2C,
	BUS_TYPE_SPI,
	BUS_TYPE_SPI_V2,
};

enum _FTS_TOUCH_ETYPE {
	TOUCH_DEFAULT = 0x00,
	TOUCH_EVENT_NUM = 0x02,
	TOUCH_FW_INIT = 0x81,
	TOUCH_IGNORE = 0xFE,
	TOUCH_ERROR = 0xFF,
};

static const struct regulator_bulk_data fts_tp_supplies[] = {
	{ .supply = "vdd" },
	{ .supply = "iovdd" },
};

struct device;
struct input_id;

int fts_probe(struct device *dev, int irq, const struct input_id *id);
void fts_remove(struct device *dev, int irq, const struct input_id *id);

/* Global variable or extern global variabls/functions */
extern struct fts_ts_data *fts_data;

/* communication interface */
int fts_read(u8 *cmd, u32 cmdlen, u8 *data, u32 datalen);
int fts_read_reg(u8 addr, u8 *value);
int fts_write(u8 *writebuf, u32 writelen);
int fts_write_reg(u8 addr, u8 value);
void fts_hid2std(void);
int fts_bus_init(struct fts_ts_data *ts_data);
int fts_bus_exit(struct fts_ts_data *ts_data);
int fts_spi_transfer_direct(u8 *writebuf, u32 writelen, u8 *readbuf,
			    u32 readlen);

/* Point Report Check*/
int fts_point_report_check_init(struct fts_ts_data *ts_data);
int fts_point_report_check_exit(struct fts_ts_data *ts_data);
void fts_prc_queue_work(struct fts_ts_data *ts_data);

/* FW upgrade */
int fts_fwupg_init(struct fts_ts_data *ts_data);
int fts_fwupg_exit(struct fts_ts_data *ts_data);
int fts_fw_resume(bool need_reset);
int fts_fw_recovery(void);

/* Other */
int fts_request_handle_reset(struct fts_ts_data *ts_data, int hdelayms);
int fts_check_cid(struct fts_ts_data *ts_data, u8 id_h);
int fts_wait_tp_to_valid(void);
void fts_release_all_finger(void);

void fts_irq_disable(void);
void fts_irq_enable(void);
#endif /* __LINUX_FOCALTECH_CORE_H__ */
