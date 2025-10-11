/*
 * FocalTech fts TouchScreen driver.
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

#include "focaltech.h"

#define FTS_CMD_RESET				0x07
#define FTS_ROMBOOT_CMD_SET_PRAM_ADDR		0xAD
#define FTS_ROMBOOT_CMD_SET_PRAM_ADDR_LEN	4
#define FTS_ROMBOOT_CMD_WRITE			0xAE
#define FTS_ROMBOOT_CMD_START_APP		0x08
#define FTS_DELAY_PRAMBOOT_START		100
#define FTS_ROMBOOT_CMD_ECC			0xCC
#define FTS_PRAM_SADDR				0x000000
#define FTS_DRAM_SADDR				0xD00000

#define FTS_CMD_READ_FWCFG			0xA8

#define FTS_CMD_READ				0x03
#define FTS_CMD_READ_DELAY			1
#define FTS_CMD_READ_LEN			4
#define FTS_CMD_READ_LEN_SPI			6
#define FTS_CMD_FLASH_TYPE			0x05
#define FTS_CMD_FLASH_MODE			0x09
#define FLASH_MODE_WRITE_FLASH_VALUE		0x0A
#define FLASH_MODE_UPGRADE_VALUE		0x0B
#define FLASH_MODE_LIC_VALUE			0x0C
#define FLASH_MODE_PARAM_VALUE			0x0D
#define FTS_CMD_ERASE_APP			0x61
#define FTS_REASE_APP_DELAY			1350
#define FTS_ERASE_SECTOR_DELAY			60
#define FTS_RETRIES_REASE			50
#define FTS_RETRIES_DELAY_REASE			400
#define FTS_CMD_FLASH_STATUS			0x6A
#define FTS_CMD_FLASH_STATUS_LEN		2
#define FTS_CMD_FLASH_STATUS_NOP		0x0000
#define FTS_CMD_FLASH_STATUS_ECC_OK		0xF055
#define FTS_CMD_FLASH_STATUS_ERASE_OK		0xF0AA
#define FTS_CMD_FLASH_STATUS_WRITE_OK		0x1000
#define FTS_CMD_ECC_INIT			0x64
#define FTS_CMD_ECC_CAL				0x65
#define FTS_CMD_ECC_CAL_LEN			7
#define FTS_RETRIES_ECC_CAL			10
#define FTS_RETRIES_DELAY_ECC_CAL		50
#define FTS_CMD_ECC_READ			0x66
#define FTS_CMD_DATA_LEN			0xB0
#define FTS_CMD_APP_DATA_LEN_INCELL		0x7A
#define FTS_CMD_DATA_LEN_LEN			4
#define FTS_CMD_SET_WFLASH_ADDR			0xAB
#define FTS_CMD_SET_RFLASH_ADDR			0xAC
#define FTS_LEN_SET_ADDR			4
#define FTS_CMD_WRITE				0xBF
#define FTS_RETRIES_WRITE			100
#define FTS_RETRIES_DELAY_WRITE			1
#define FTS_CMD_WRITE_LEN			6
#define FTS_DELAY_READ_ID			20
#define FTS_DELAY_UPGRADE_RESET			80
#define PRAMBOOT_MIN_SIZE			0x120
#define PRAMBOOT_MAX_SIZE			(64 * 1024)
#define FTS_FLASH_PACKET_LENGTH			32 /* max=128 */
#define FTS_MAX_LEN_ECC_CALC			0xFFFE /* must be even */
#define FTS_MIN_LEN				0x120
#define FTS_MAX_LEN_FILE			(256 * 1024)
#define FTS_MAX_LEN_APP				(64 * 1024)
#define FTS_MAX_LEN_SECTOR			(4 * 1024)
#define FTS_CONIFG_VENDORID_OFF			0x04
#define FTS_CONIFG_MODULEID_OFF			0x1E
#define FTS_CONIFG_PROJECTID_OFF		0x20
#define FTS_APPINFO_OFF				0x100
#define FTS_APPINFO_APPLEN_OFF			0x00
#define FTS_APPINFO_APPLEN2_OFF			0x12
#define FTS_REG_UPGRADE				0xFC
#define FTS_REG_UPGRADE2			0xBC
#define FTS_UPGRADE_AA				0xAA
#define FTS_UPGRADE_55				0x55
#define FTS_DELAY_UPGRADE_AA			10
#define FTS_UPGRADE_LOOP			30
#define FTS_HEADER_LEN				32
#define FTS_FW_BIN_FILEPATH			"/sdcard/"
#define FTS_FW_IDE_SIG				"IDE_"
#define FTS_FW_IDE_SIG_LEN			4
#define MAX_MODULE_VENDOR_NAME_LEN		16

#define FTS_ROMBOOT_CMD_ECC_NEW_LEN		7
#define FTS_ECC_FINISH_TIMEOUT			100
#define FTS_ROMBOOT_CMD_ECC_FINISH		0xCE
#define FTS_ROMBOOT_CMD_ECC_FINISH_OK_A5	0xA5
#define FTS_ROMBOOT_CMD_ECC_FINISH_OK_00	0x00
#define FTS_ROMBOOT_CMD_ECC_READ		0xCD
#define AL2_FCS_COEF				((1 << 15) + (1 << 10) + (1 << 3))

#define FTS_APP_INFO_OFFSET			0x100

enum FW_STATUS {
	FTS_RUN_IN_ERROR,
	FTS_RUN_IN_APP,
	FTS_RUN_IN_ROM,
	FTS_RUN_IN_PRAM,
	FTS_RUN_IN_BOOTLOADER,
};

enum FW_FLASH_MODE {
	FLASH_MODE_APP,
	FLASH_MODE_LIC,
	FLASH_MODE_PARAM,
	FLASH_MODE_ALL,
};

enum ECC_CHECK_MODE {
	ECC_CHECK_MODE_XOR,
	ECC_CHECK_MODE_CRC16,
};

enum UPGRADE_SPEC {
	UPGRADE_SPEC_V_1_0 = 0x0100,
	UPGRADE_SPEC_V_1_1 = 0x0101,
	UPGRADE_SPEC_V_1_2 = 0x0102,
};

struct upgrade_func {
	u16 ctype[FTS_MAX_COMPATIBLE_TYPE];
	u32 fwveroff;
	u32 fwcfgoff;
	u32 appoff;
	u32 licoff;
	u32 paramcfgoff;
	u32 paramcfgveroff;
	u32 paramcfg2off;
	int pram_ecc_check_mode;
	int fw_ecc_check_mode;
	int upgspec_version;
	bool new_return_value_from_ic;
	bool appoff_handle_in_ic;
	bool is_reset_register_BC;
	bool read_boot_id_need_reset;
	bool hid_supported;
	bool pramboot_supported;
	u8 *pramboot;
	u32 pb_length;
	int (*init)(u8 *, u32);
	int (*write_pramboot_private)(void);
	int (*upgrade)(u8 *, u32);
	int (*get_hlic_ver)(u8 *);
	int (*lic_upgrade)(u8 *, u32);
	int (*param_upgrade)(u8 *, u32);
	int (*force_upgrade)(u8 *, u32);
};

struct upgrade_setting_nf {
	u8 rom_idh;
	u8 rom_idl;
	u16 reserved;
	u32 app2_offset;
	u32 ecclen_max;
	u8 eccok_val;
	u8 upgsts_boot;
	u8 delay_init;
	u8 spi_pe;
	u8 length_coefficient;
	u8 fd_check;
	u8 drwr_support;
	u8 ecc_delay;
};

struct upgrade_module {
	int id;
	char vendor_name[MAX_MODULE_VENDOR_NAME_LEN];
	u8 *fw_file;
	u32 fw_len;
};

struct fts_upgrade {
	struct fts_ts_data *ts_data;
	struct upgrade_module *module_info;
	struct upgrade_func *func;
	struct upgrade_setting_nf *setting_nf;
	int module_id;
	bool fw_from_request;
	u8 *fw;
	u32 fw_length;
	u8 *lic;
	u32 lic_length;
};

#define FTS_READ_BOOT_ID_TIMEOUT	3
#define FTS_FLASH_PACKET_LENGTH_SPI_LOW	(4 * 1024 - 4)
#define FTS_FLASH_PACKET_LENGTH_SPI	(32 * 1024 - 16)


struct upgrade_setting_nf upgrade_setting_list[] = {
	{ 0x87, 0x19, 0, (64 * 1024), (128 * 1024), 0x00, 0x02, 8, 1, 1, 1, 0, 0 },
	{ 0x86, 0x22, 0, (64 * 1024), (128 * 1024), 0x00, 0x02, 8, 1, 1, 0, 0, 0 },
	{ 0x87, 0x56, 0, (88 * 1024), 32766, 0xA5, 0x01, 8, 0, 2, 0, 1, 0 },
	{ 0x80, 0x09, 0, (88 * 1024), 32766, 0xA5, 0x01, 8, 0, 2, 0, 1, 0 },
	{ 0x86, 0x32, 0, (64 * 1024), (128 * 1024), 0xA5, 0x01, 12, 0, 1, 0, 0, 0 },
	{ 0x86, 0x42, 0, (64 * 1024), (128 * 1024), 0xA5, 0x01, 12, 0, 1, 0, 0, 0 },
	{ 0x87, 0x20, 0, (88 * 1024), (128 * 1024), 0xA5, 0x01, 8, 0, 2, 0, 1, 0 },
	{ 0x87, 0x22, 0, (88 * 1024), (128 * 1024), 0xA5, 0x01, 8, 0, 2, 0, 1, 0 },
	{ 0x82, 0x01, 0, (96 * 1024), (128 * 1024), 0xA5, 0x01, 8, 0, 2, 0, 0, 0 },
	{ 0xF0, 0xC6, 0, (84 * 1024), (128 * 1024), 0xA5, 0x01, 8, 0, 2, 0, 1, 0 },
	{ 0x56, 0x62, 0, (128 * 1024), (128 * 1024), 0xA5, 0x01, 8, 0, 4, 0, 0, 5 },
	{ 0x82, 0x05, 0, (120 * 1024), (128 * 1024), 0xA5, 0x01, 8, 0, 2, 0, 0, 0 },
};

struct fts_upgrade *fwupgrade;

static int fts_check_bootid(void)
{
	int ret = 0;
	u8 cmd = 0;
	u8 id[2] = { 0 };
	struct fts_upgrade *upg = fwupgrade;
	struct ft_chip_t *chip_id;

	if (!upg || !upg->ts_data || !upg->setting_nf) {
		dev_err(fts_data->dev, "upgrade/ts_data/setting_nf is null");
		return -EINVAL;
	}

	chip_id = &upg->ts_data->ic_info.ids;

	cmd = FTS_CMD_READ_ID;
	ret = fts_read(&cmd, 1, id, 2);
	if (ret < 0) {
		dev_err(fts_data->dev, "read boot id(0x%02x 0x%02x) fail",
			id[0], id[1]);
		return ret;
	}

	dev_info(fts_data->dev, "read boot id:0x%02x 0x%02x", id[0], id[1]);
	if ((chip_id->rom_idh == id[0]) && (chip_id->rom_idl == id[1]))
		return 0;

	return -EIO;
}

static int fts_enter_into_boot(void)
{
	int ret = 0;
	int i = 0;
	int j = 0;
	u8 cmd[2] = { 0 };
	struct fts_upgrade *upg = fwupgrade;

	if (!upg || !upg->ts_data || !upg->setting_nf) {
		dev_err(fts_data->dev, "upgrade/ts_data/setting_nf is null");
		return -EINVAL;
	}

	dev_info(fts_data->dev, "enter into boot environment");
	for (i = 0; i < FTS_UPGRADE_LOOP; i++) {
		/* hardware tp reset to boot */
		fts_request_handle_reset(fts_data, 0);
		mdelay(upg->setting_nf->delay_init + i);

		/* enter into boot & check boot id*/
		for (j = 0; j < FTS_READ_BOOT_ID_TIMEOUT; j++) {
			cmd[0] = FTS_CMD_START1;
			ret = fts_write(cmd, 1);
			if (ret >= 0) {
				mdelay(upg->setting_nf->delay_init);
				ret = fts_check_bootid();
				if (0 == ret) {
					dev_info(fts_data->dev,
						 "boot id check pass, retry=%d",
						 i);
					return 0;
				}
			}
		}
	}

	return -EIO;
}

static bool fts_check_fast_download(void)
{
	int ret = 0;
	u8 cmd[6] = { 0xF2, 0x00, 0x78, 0x0A, 0x00, 0x02 };
	u8 value = 0;
	u8 value2[2] = { 0 };

	ret = fts_read_reg(0xdb, &value);
	if (ret < 0) {
		dev_err(fts_data->dev, "read 0xdb fail");
		goto read_err;
	}

	ret = fts_read(cmd, 6, value2, 2);
	if (ret < 0) {
		dev_err(fts_data->dev, "read f2 fail");
		goto read_err;
	}

	dev_info(fts_data->dev, "0xdb = 0x%x, 0xF2 = 0x%x", value, value2[0]);
	if ((value >= 0x18) && (value2[0] == 0x55)) {
		dev_info(fts_data->dev, "IC support fast-download");
		return true;
	}

read_err:
	dev_info(fts_data->dev, "IC not support fast-download");
	return false;
}

static int fts_dpram_write_pe(u32 saddr, const u8 *buf, u32 len, bool wpram)
{
	int ret = 0;
	int i = 0;
	int j = 0;
	u8 *cmd = NULL;
	u32 addr = 0;
	u32 offset = 0;
	u32 remainder = 0;
	u32 packet_number = 0;
	u32 packet_len = 0;
	u32 packet_size = FTS_FLASH_PACKET_LENGTH_SPI;
	bool fd_support = true;
	struct fts_upgrade *upg = fwupgrade;

	dev_info(fts_data->dev, "dpram write");
	if (!upg || !upg->ts_data || !upg->setting_nf) {
		dev_err(fts_data->dev, "upgrade/ts_data/setting_nf is null");
		return -EINVAL;
	}

	if (!buf) {
		dev_err(fts_data->dev, "fw buf is null");
		return -EINVAL;
	}

	if ((len < FTS_MIN_LEN) || (len > upg->setting_nf->app2_offset)) {
		dev_err(fts_data->dev, "fw length(%d) fail", len);
		return -EINVAL;
	}

	if (upg->setting_nf->fd_check) {
		fd_support = fts_check_fast_download();
		if (!fd_support)
			packet_size = FTS_FLASH_PACKET_LENGTH_SPI_LOW;
	}

	cmd = vmalloc(packet_size + FTS_CMD_WRITE_LEN + 1);
	if (NULL == cmd) {
		dev_err(fts_data->dev,
			"malloc memory for pram write buffer fail");
		return -ENOMEM;
	}
	memset(cmd, 0, packet_size + FTS_CMD_WRITE_LEN + 1);

	packet_number = len / packet_size;
	remainder = len % packet_size;
	if (remainder > 0)
		packet_number++;
	packet_len = packet_size;
	dev_info(fts_data->dev, "write data, num:%d remainder:%d",
		 packet_number, remainder);

	cmd[0] = FTS_ROMBOOT_CMD_WRITE;
	for (i = 0; i < packet_number; i++) {
		offset = i * packet_size;
		addr = saddr + offset;
		cmd[1] = BYTE_OFF_16(addr);
		cmd[2] = BYTE_OFF_8(addr);
		cmd[3] = BYTE_OFF_0(addr);

		/* last packet */
		if ((i == (packet_number - 1)) && remainder)
			packet_len = remainder;
		cmd[4] = BYTE_OFF_8(packet_len);
		cmd[5] = BYTE_OFF_0(packet_len);

		for (j = 0; j < packet_len; j++)
			cmd[FTS_CMD_WRITE_LEN + j] = buf[offset + j];

		ret = fts_write(&cmd[0], FTS_CMD_WRITE_LEN + packet_len);
		if (ret < 0) {
			dev_err(fts_data->dev, "write fw to pram(%d) fail", i);
			goto write_pram_err;
		}

		if (!fd_support)
			mdelay(3);
	}

write_pram_err:
	if (cmd) {
		vfree(cmd);
		cmd = NULL;
	}
	return ret;
}

static int fts_dpram_write(u32 saddr, const u8 *buf, u32 len, bool wpram)
{
	int ret = 0;
	int i = 0;
	int j = 0;
	u8 *cmd = NULL;
	u32 addr = 0;
	u32 baseaddr = wpram ? FTS_PRAM_SADDR : FTS_DRAM_SADDR;
	u32 offset = 0;
	u32 remainder = 0;
	u32 packet_number = 0;
	u32 packet_len = 0;
	u32 packet_size = FTS_FLASH_PACKET_LENGTH_SPI;
	struct fts_upgrade *upg = fwupgrade;

	dev_info(fts_data->dev, "dpram write");
	if (!upg || !upg->ts_data || !upg->setting_nf) {
		dev_err(fts_data->dev, "upgrade/ts_data/setting_nf is null");
		return -EINVAL;
	}

	if (!buf) {
		dev_err(fts_data->dev, "fw buf is null");
		return -EINVAL;
	}

	if ((len < FTS_MIN_LEN) || (len > upg->setting_nf->app2_offset)) {
		dev_err(fts_data->dev, "fw length(%d) fail", len);
		return -EINVAL;
	}

	cmd = vmalloc(packet_size + FTS_CMD_WRITE_LEN + 1);
	if (NULL == cmd) {
		dev_err(fts_data->dev,
			"malloc memory for pram write buffer fail");
		return -ENOMEM;
	}
	memset(cmd, 0, packet_size + FTS_CMD_WRITE_LEN + 1);

	packet_number = len / packet_size;
	remainder = len % packet_size;
	if (remainder > 0)
		packet_number++;
	packet_len = packet_size;
	dev_info(fts_data->dev, "write data, num:%d remainder:%d",
		 packet_number, remainder);

	for (i = 0; i < packet_number; i++) {
		offset = i * packet_size;
		addr = saddr + offset + baseaddr;
		/* last packet */
		if ((i == (packet_number - 1)) && remainder)
			packet_len = remainder;

		/* set pram address */
		cmd[0] = FTS_ROMBOOT_CMD_SET_PRAM_ADDR;
		cmd[1] = BYTE_OFF_16(addr);
		cmd[2] = BYTE_OFF_8(addr);
		cmd[3] = BYTE_OFF_0(addr);
		ret = fts_write(&cmd[0], FTS_ROMBOOT_CMD_SET_PRAM_ADDR_LEN);
		if (ret < 0) {
			dev_err(fts_data->dev, "set pram(%d) addr(%d) fail", i,
				addr);
			goto write_pram_err;
		}

		/* write pram data */
		cmd[0] = FTS_ROMBOOT_CMD_WRITE;
		for (j = 0; j < packet_len; j++) {
			cmd[1 + j] = buf[offset + j];
		}
		ret = fts_write(&cmd[0], 1 + packet_len);
		if (ret < 0) {
			dev_err(fts_data->dev, "write fw to pram(%d) fail", i);
			goto write_pram_err;
		}
	}

write_pram_err:
	if (cmd) {
		vfree(cmd);
		cmd = NULL;
	}
	return ret;
}

static int fts_ecc_cal_tp(u32 ecc_saddr, u32 ecc_len, u16 *ecc_value)
{
	int ret = 0;
	int i = 0;
	u8 cmd[FTS_ROMBOOT_CMD_ECC_NEW_LEN] = { 0 };
	u8 value[2] = { 0 };
	struct fts_upgrade *upg = fwupgrade;

	dev_info(fts_data->dev, "ecc calc in tp");
	if (!upg || !upg->ts_data || !upg->setting_nf) {
		dev_err(fts_data->dev, "upgrade/ts_data/setting_nf is null");
		return -EINVAL;
	}

	cmd[0] = FTS_ROMBOOT_CMD_ECC;
	cmd[1] = BYTE_OFF_16(ecc_saddr);
	cmd[2] = BYTE_OFF_8(ecc_saddr);
	cmd[3] = BYTE_OFF_0(ecc_saddr);
	cmd[4] = BYTE_OFF_16(ecc_len);
	cmd[5] = BYTE_OFF_8(ecc_len);
	cmd[6] = BYTE_OFF_0(ecc_len);

	/* make boot to calculate ecc in pram */
	ret = fts_write(cmd, FTS_ROMBOOT_CMD_ECC_NEW_LEN);
	if (ret < 0) {
		dev_err(fts_data->dev, "ecc calc cmd fail");
		return ret;
	}
	mdelay(2);

	/* wait boot calculate ecc finish */
	if (upg->setting_nf->ecc_delay) {
		mdelay(upg->setting_nf->ecc_delay);
	} else {
		cmd[0] = FTS_ROMBOOT_CMD_ECC_FINISH;
		for (i = 0; i < FTS_ECC_FINISH_TIMEOUT; i++) {
			ret = fts_read(cmd, 1, value, 1);
			if (ret < 0) {
				dev_err(fts_data->dev, "ecc finish cmd fail");
				return ret;
			}
			if (upg->setting_nf->eccok_val == value[0])
				break;
			mdelay(1);
		}
		if (i >= FTS_ECC_FINISH_TIMEOUT) {
			dev_err(fts_data->dev,
				"wait ecc finish timeout,ecc_finish=%x",
				value[0]);
			return -EIO;
		}
	}

	/* get ecc value calculate in boot */
	cmd[0] = FTS_ROMBOOT_CMD_ECC_READ;
	ret = fts_read(cmd, 1, value, 2);
	if (ret < 0) {
		dev_err(fts_data->dev, "ecc read cmd fail");
		return ret;
	}

	*ecc_value = ((u16)(value[0] << 8) + value[1]) & 0x0000FFFF;
	return 0;
}

static int fts_ecc_cal_host(const u8 *data, u32 data_len, u16 *ecc_value)
{
	u16 ecc = 0;
	u32 i = 0;
	u32 j = 0;
	u16 al2_fcs_coef = AL2_FCS_COEF;

	for (i = 0; i < data_len; i += 2) {
		ecc ^= ((data[i] << 8) | (data[i + 1]));
		for (j = 0; j < 16; j++) {
			if (ecc & 0x01)
				ecc = (u16)((ecc >> 1) ^ al2_fcs_coef);
			else
				ecc >>= 1;
		}
	}

	*ecc_value = ecc & 0x0000FFFF;
	return 0;
}

static int fts_ecc_check(const u8 *buf, u32 len, u32 ecc_saddr)
{
	int ret = 0;
	int i = 0;
	u16 ecc_in_host = 0;
	u16 ecc_in_tp = 0;
	int packet_length = 0;
	int packet_number = 0;
	int packet_remainder = 0;
	int offset = 0;
	u32 packet_size = FTS_MAX_LEN_FILE;
	struct fts_upgrade *upg = fwupgrade;

	dev_info(fts_data->dev, "ecc check");
	if (!upg || !upg->ts_data || !upg->setting_nf) {
		dev_err(fts_data->dev, "upgrade/ts_data/setting_nf is null");
		return -EINVAL;
	}

	if (upg->setting_nf->ecclen_max)
		packet_size = upg->setting_nf->ecclen_max;

	packet_number = len / packet_size;
	packet_remainder = len % packet_size;
	if (packet_remainder)
		packet_number++;
	packet_length = packet_size;

	for (i = 0; i < packet_number; i++) {
		/* last packet */
		if ((i == (packet_number - 1)) && packet_remainder)
			packet_length = packet_remainder;

		ret = fts_ecc_cal_host(buf + offset, packet_length,
				       &ecc_in_host);
		if (ret < 0) {
			dev_err(fts_data->dev, "ecc in host calc fail");
			return ret;
		}

		ret = fts_ecc_cal_tp(ecc_saddr + offset, packet_length,
				     &ecc_in_tp);
		if (ret < 0) {
			dev_err(fts_data->dev, "ecc in tp calc fail");
			return ret;
		}

		dev_dbg(fts_data->dev, "ecc in tp:%04x,host:%04x,i:%d",
			ecc_in_tp, ecc_in_host, i);
		if (ecc_in_tp != ecc_in_host) {
			dev_err(fts_data->dev,
				"ecc_in_tp(%x) != ecc_in_host(%x), ecc check fail",
				ecc_in_tp, ecc_in_host);
			return -EIO;
		}

		offset += packet_length;
	}

	return 0;
}

static int fts_pram_write_ecc(const u8 *buf, u32 len)
{
	int ret = 0;
	u32 pram_app_size = 0;
	u16 code_len = 0;
	u16 code_len_n = 0;
	u32 pram_start_addr = 0;
	struct fts_upgrade *upg = fwupgrade;

	dev_info(fts_data->dev, "begin to write pram app(bin len:%d)", len);
	if (!upg || !upg->setting_nf) {
		dev_err(fts_data->dev, "upgrade/setting_nf is null");
		return -EINVAL;
	}

	/* get pram app length */
	code_len = ((u16)buf[FTS_APP_INFO_OFFSET + 0] << 8) +
		   buf[FTS_APP_INFO_OFFSET + 1];
	code_len_n = ((u16)buf[FTS_APP_INFO_OFFSET + 2] << 8) +
		     buf[FTS_APP_INFO_OFFSET + 3];
	if ((code_len + code_len_n) != 0xFFFF) {
		dev_err(fts_data->dev, "pram code len(%x %x) fail", code_len,
			code_len_n);
		return -EINVAL;
	}

	pram_app_size = ((u32)code_len) * upg->setting_nf->length_coefficient;
	dev_info(fts_data->dev, "pram app length in fact:%d", pram_app_size);

	/* write pram */
	if (upg->setting_nf->spi_pe)
		ret = fts_dpram_write_pe(pram_start_addr, buf, pram_app_size,
					 true);
	else
		ret = fts_dpram_write(pram_start_addr, buf, pram_app_size,
				      true);
	if (ret < 0) {
		dev_err(fts_data->dev, "write pram fail");
		return ret;
	}

	/* check ecc */
	ret = fts_ecc_check(buf, pram_app_size, pram_start_addr);
	if (ret < 0) {
		dev_err(fts_data->dev, "pram ecc check fail");
		return ret;
	}

	dev_info(fts_data->dev, "pram app write successfully");
	return 0;
}

static int fts_dram_write_ecc(const u8 *buf, u32 len)
{
	int ret = 0;
	u32 dram_size = 0;
	u32 pram_app_size = 0;
	u32 dram_start_addr = 0;
	u16 const_len = 0;
	u16 const_len_n = 0;
	const u8 *dram_buf = NULL;
	struct fts_upgrade *upg = fwupgrade;

	dev_info(fts_data->dev, "begin to write dram data(bin len:%d)", len);
	if (!upg || !upg->setting_nf) {
		dev_err(fts_data->dev, "upgrade/setting_nf is null");
		return -EINVAL;
	}

	/* get dram data length */
	const_len = ((u16)buf[FTS_APP_INFO_OFFSET + 0x8] << 8) +
		    buf[FTS_APP_INFO_OFFSET + 0x9];
	const_len_n = ((u16)buf[FTS_APP_INFO_OFFSET + 0x0A] << 8) +
		      buf[FTS_APP_INFO_OFFSET + 0x0B];
	if (((const_len + const_len_n) != 0xFFFF) || (const_len == 0)) {
		dev_info(fts_data->dev, "no support dram,const len(%x %x)",
			 const_len, const_len_n);
		return 0;
	}

	dram_size = ((u32)const_len) * upg->setting_nf->length_coefficient;
	pram_app_size = ((u32)(((u16)buf[FTS_APP_INFO_OFFSET + 0] << 8) +
			       buf[FTS_APP_INFO_OFFSET + 1]));
	pram_app_size = pram_app_size * upg->setting_nf->length_coefficient;

	dram_buf = buf + pram_app_size;
	dev_info(fts_data->dev, "dram buf length in fact:%d,offset:%d",
		 dram_size, pram_app_size);
	/* write pram */
	ret = fts_dpram_write(dram_start_addr, dram_buf, dram_size, false);
	if (ret < 0) {
		dev_err(fts_data->dev, "write dram fail");
		return ret;
	}

	/* check ecc */
	ret = fts_ecc_check(dram_buf, dram_size, dram_start_addr);
	if (ret < 0) {
		dev_err(fts_data->dev, "dram ecc check fail");
		return ret;
	}

	dev_info(fts_data->dev, "dram data write successfully");
	return 0;
}

static int fts_pram_start(void)
{
	int ret = 0;
	u8 cmd = FTS_ROMBOOT_CMD_START_APP;

	dev_info(fts_data->dev, "remap to start pram");
	ret = fts_write(&cmd, 1);
	if (ret < 0) {
		dev_err(fts_data->dev, "write start pram cmd fail");
		return ret;
	}

	msleep(10);
	return 0;
}

/*
 * description: download fw to IC and run
 *
 * param - buf: const, fw data buffer
 *         len: length of fw
 *
 * return 0 if success, otherwise return error code
 */
static int fts_fw_write_start(const u8 *buf, u32 len, bool need_reset)
{
	int ret = 0;
	struct fts_upgrade *upg = fwupgrade;

	dev_info(fts_data->dev, "begin to write and start fw(bin len:%d)", len);
	if (!upg || !upg->ts_data || !upg->setting_nf) {
		dev_err(fts_data->dev, "upgrade/ts_data/setting_nf is null");
		return -EINVAL;
	}

	upg->ts_data->fw_is_running = false;

	if (need_reset) {
		/* enter into boot environment */
		ret = fts_enter_into_boot();
		if (ret < 0) {
			dev_err(fts_data->dev,
				"enter into boot environment fail");
			return ret;
		}
	}

	/* write pram */
	ret = fts_pram_write_ecc(buf, len);
	if (ret < 0) {
		dev_err(fts_data->dev, "write pram fail");
		return ret;
	}

	if (upg->setting_nf->drwr_support) {
		/* write dram */
		ret = fts_dram_write_ecc(buf, len);
		if (ret < 0) {
			dev_err(fts_data->dev, "write dram fail");
			return ret;
		}
	}

	/* remap pram and run fw */
	ret = fts_pram_start();
	if (ret < 0) {
		dev_err(fts_data->dev, "pram start fail");
		return ret;
	}

	upg->ts_data->fw_is_running = true;
	dev_info(fts_data->dev, "fw download successfully");
	return 0;
}

static int fts_fw_download(const u8 *buf, u32 len, bool need_reset)
{
	int ret = 0;
	int i = 0;
	struct fts_upgrade *upg = fwupgrade;

	dev_info(fts_data->dev, "fw upgrade download function");
	if (!upg || !upg->ts_data || !upg->setting_nf) {
		dev_err(fts_data->dev, "upgrade/ts_data/setting_nf is null");
		return -EINVAL;
	}

	if (!buf || (len < FTS_MIN_LEN)) {
		dev_err(fts_data->dev, "fw/len(%d) is invalid", len);
		return -EINVAL;
	}

	upg->ts_data->fw_loading = 1;
	fts_irq_disable();

	for (i = 0; i < 3; i++) {
		dev_info(fts_data->dev, "fw download times:%d", i + 1);
		ret = fts_fw_write_start(buf, len, need_reset);
		if (0 == ret)
			break;
	}
	if (i >= 3) {
		dev_err(fts_data->dev, "fw download fail");
		ret = -EIO;
		goto err_fw_download;
	}

	ret = 0;
err_fw_download:
	fts_irq_enable();
	upg->ts_data->fw_loading = 0;

	return ret;
}

int fts_fw_resume(bool need_reset)
{
	int ret = 0;
	struct fts_upgrade *upg = fwupgrade;
	const struct firmware *fw = NULL;
	bool get_fw_i_flag = true;
	const u8 *fw_buf = NULL;
	u32 fwlen = 0;

	dev_info(fts_data->dev, "fw upgrade resume function");
	if (!upg || !upg->fw) {
		dev_err(fts_data->dev, "upg/fw is null");
		return -EINVAL;
	}

	if (upg->ts_data->fw_loading) {
		dev_info(fts_data->dev, "fw is loading, not download again");
		return -EINVAL;
	}

	ret = request_firmware(&fw, fts_data->firmware_path, upg->ts_data->dev);
	if (ret == 0) {
		dev_info(fts_data->dev,
			 "firmware(%s) request successfully", fts_data->firmware_path);
		fw_buf = fw->data;
		fwlen = fw->size;
		get_fw_i_flag = false;
	} else {
		dev_err(fts_data->dev,
			"%s:firmware(%s) request fail,ret=%d\n",
			__func__, fts_data->firmware_path, ret);
	}

	if (get_fw_i_flag) {
		dev_info(fts_data->dev, "download fw from bootimage");
		fw_buf = upg->fw;
		fwlen = upg->fw_length;
	}

	ret = fts_fw_download(fw_buf, fwlen, need_reset);
	if (ret < 0) {
		dev_err(fts_data->dev, "fw resume download failed");
	}

	if (fw != NULL) {
		release_firmware(fw);
		fw = NULL;
	}

	return ret;
}

int fts_fw_recovery(void)
{
	int ret = 0;
	u8 boot_state = 0;
	u8 chip_id = 0;
	struct fts_upgrade *upg = fwupgrade;

	dev_info(fts_data->dev, "check if boot recovery");
	if (!upg || !upg->ts_data || !upg->setting_nf) {
		dev_err(fts_data->dev, "upg/ts_data/setting_nf is null");
		return -EINVAL;
	}

	if (upg->ts_data->fw_loading) {
		dev_info(fts_data->dev, "fw is loading, not download again");
		return -EINVAL;
	}

	upg->ts_data->fw_is_running = false;
	ret = fts_check_bootid();
	if (ret < 0) {
		dev_err(fts_data->dev, "check boot id fail");
		upg->ts_data->fw_is_running = true;
		return ret;
	}

	ret = fts_read_reg(0xD0, &boot_state);
	if (ret < 0) {
		dev_err(fts_data->dev, "read boot state failed, ret=%d", ret);
		upg->ts_data->fw_is_running = true;
		return ret;
	}

	if (boot_state != upg->setting_nf->upgsts_boot) {
		dev_info(fts_data->dev, "not in boot mode(0x%x),exit",
			 boot_state);
		upg->ts_data->fw_is_running = true;
		return -EIO;
	}

	dev_info(fts_data->dev, "abnormal situation,need download fw");
	ret = fts_fw_resume(false);
	if (ret < 0) {
		dev_err(fts_data->dev, "fts_fw_resume fail");
		return ret;
	}

	ret = fts_read_reg(FTS_REG_CHIP_ID, &chip_id);
	dev_info(fts_data->dev, "read chip id:0x%02x", chip_id);

	fts_wait_tp_to_valid();

	dev_info(fts_data->dev, "boot recovery pass");
	return ret;
}

static int fts_get_fw_file_via_request_firmware(struct fts_upgrade *upg)
{
	int ret = 0;
	const struct firmware *fw = NULL;
	u8 *tmpbuf = NULL;

	ret = request_firmware(&fw, fts_data->firmware_path, upg->ts_data->dev);
	if (0 == ret) {
		dev_info(fts_data->dev, "firmware(%s) request successfully",
			 fts_data->firmware_path);
		tmpbuf = vmalloc(fw->size);
		if (NULL == tmpbuf) {
			dev_err(fts_data->dev, "fw buffer vmalloc fail");
			ret = -ENOMEM;
		} else {
			memcpy(tmpbuf, fw->data, fw->size);
			upg->fw = tmpbuf;
			upg->fw_length = fw->size;
			upg->fw_from_request = 1;
		}
	} else {
		dev_info(fts_data->dev, "firmware(%s) request fail,ret=%d",
			 fts_data->firmware_path, ret);
	}

	if (fw != NULL) {
		release_firmware(fw);
		fw = NULL;
	}

	return ret;
}

static int fts_fwupg_get_fw_file(struct fts_upgrade *upg)
{
	int ret = 0;
	bool get_fw_i_flag = false;

	dev_dbg(fts_data->dev, "get upgrade fw file");
	if (!upg || !upg->ts_data) {
		dev_err(fts_data->dev, "upg/ts_data is null");
		return -EINVAL;
	}

	/* 500 */
	msleep(10000);
	ret = fts_get_fw_file_via_request_firmware(upg);
	if (ret != 0)
		get_fw_i_flag = true;

	dev_info(fts_data->dev, "upgrade fw file len:%d", upg->fw_length);
	if (upg->fw_length < FTS_MIN_LEN) {
		dev_err(fts_data->dev, "fw file len(%d) fail", upg->fw_length);
		return -ENODATA;
	}

	return ret;
}

static void fts_fwupg_work(struct work_struct *work)
{
	int ret = 0;
	u8 chip_id = 0;
	struct fts_upgrade *upg = fwupgrade;

	dev_info(fts_data->dev, "fw upgrade work function");
	if (!upg || !upg->ts_data) {
		dev_err(fts_data->dev, "upg/ts_data is null");
		return;
	}

	/* get fw */
	ret = fts_fwupg_get_fw_file(upg);
	if (ret < 0) {
		dev_err(fts_data->dev, "get file fail, can't upgrade");
		return;
	}

	if (upg->ts_data->fw_loading) {
		dev_info(fts_data->dev, "fw is loading, not download again");
		return;
	}

	ret = fts_fw_download(upg->fw, upg->fw_length, true);
	if (ret < 0) {
		dev_err(fts_data->dev, "fw auto download failed");
	} else {
		msleep(50);
		ret = fts_read_reg(FTS_REG_CHIP_ID, &chip_id);
		dev_info(fts_data->dev, "read chip id:0x%02x", chip_id);
	}
}

int fts_fwupg_init(struct fts_ts_data *ts_data)
{
	int i = 0;
	struct upgrade_setting_nf *setting = &upgrade_setting_list[0];
	int setting_count =
		sizeof(upgrade_setting_list) / sizeof(upgrade_setting_list[0]);

	dev_info(fts_data->dev, "fw upgrade init function");
	if (!ts_data || !ts_data->ts_workqueue) {
		dev_err(fts_data->dev,
			"ts_data/workqueue is NULL, can't run upgrade function");
		return -EINVAL;
	}

	if (0 == setting_count) {
		dev_err(fts_data->dev,
			"no upgrade settings in tp driver, init fail");
		return -ENODATA;
	}

	fwupgrade =
		(struct fts_upgrade *)kzalloc(sizeof(*fwupgrade), GFP_KERNEL);
	if (NULL == fwupgrade) {
		dev_err(fts_data->dev, "malloc memory for upgrade fail");
		return -ENOMEM;
	}

	if (1 == setting_count)
		fwupgrade->setting_nf = setting;
	else {
		for (i = 0; i < setting_count; i++) {
			setting = &upgrade_setting_list[i];
			if ((setting->rom_idh ==
			     ts_data->ic_info.ids.rom_idh) &&
			    (setting->rom_idl ==
			     ts_data->ic_info.ids.rom_idl)) {
				dev_info(
					fts_data->dev,
					"match upgrade setting,type(ID):0x%02x%02x",
					setting->rom_idh, setting->rom_idl);
				fwupgrade->setting_nf = setting;
			}
		}
	}

	if (NULL == fwupgrade->setting_nf) {
		dev_err(fts_data->dev,
			"no upgrade settings match, can't upgrade");
		kfree(fwupgrade);
		fwupgrade = NULL;
		return -ENODATA;
	}

	fwupgrade->ts_data = ts_data;
	INIT_WORK(&ts_data->fwupg_work, fts_fwupg_work);
	queue_work(ts_data->ts_workqueue, &ts_data->fwupg_work);

	return 0;
}

int fts_fwupg_exit(struct fts_ts_data *ts_data)
{
	cancel_work_sync(&ts_data->fwupg_work);

	if (fwupgrade) {
		if (fwupgrade->fw_from_request) {
			vfree(fwupgrade->fw);
			fwupgrade->fw = NULL;
		}

		kfree(fwupgrade);
		fwupgrade = NULL;
	}

	return 0;
}
