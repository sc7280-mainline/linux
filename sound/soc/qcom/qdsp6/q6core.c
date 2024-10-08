// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2011-2017, The Linux Foundation. All rights reserved.
// Copyright (c) 2018, Linaro Limited

#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/jiffies.h>
#include <linux/firmware.h>
#include <linux/soc/qcom/apr.h>
#include "q6core.h"
#include "q6dsp-errno.h"

#define ADSP_STATE_READY_TIMEOUT_MS    3000
#define Q6_READY_TIMEOUT_MS 100
#define AVCS_CMD_ADSP_EVENT_GET_STATE		0x0001290C
#define AVCS_CMDRSP_ADSP_EVENT_GET_STATE	0x0001290D
#define AVCS_GET_VERSIONS       0x00012905
#define AVCS_GET_VERSIONS_RSP   0x00012906
#define AVCS_CMD_REGISTER_TOPOLOGIES	0x00012923
#define AVCS_CMD_DEREGISTER_TOPOLOGIES	0x0001292a
#define AVCS_CMD_GET_FWK_VERSION	0x001292c
#define AVCS_CMDRSP_GET_FWK_VERSION	0x001292d

#define AVCS_MODE_DEREGISTER_ALL_CUSTOM_TOPOLOGIES	2

#define ACDB_SECTION_GPROPLUT	0x54554c504f525047
#define ACDB_SECTION_DATAPOOL	0x4c4f4f5041544144
#define ACDB_TOPOLOGIES_BLOB	0x000131a7

struct avcs_svc_info {
	uint32_t service_id;
	uint32_t version;
} __packed;

struct avcs_cmdrsp_get_version {
	uint32_t build_id;
	uint32_t num_services;
	struct avcs_svc_info svc_api_info[];
} __packed;

/* for ADSP2.8 and above */
struct avcs_svc_api_info {
	uint32_t service_id;
	uint32_t api_version;
	uint32_t api_branch_version;
} __packed;

struct avcs_cmd_register_topologies {
	uint32_t payload_addr_lsw;
	uint32_t payload_addr_msw;
	uint32_t mem_map_handle;
	uint32_t payload_size;
} __packed;

struct avcs_cmdrsp_get_fwk_version {
	uint32_t build_major_version;
	uint32_t build_minor_version;
	uint32_t build_branch_version;
	uint32_t build_subbranch_version;
	uint32_t num_services;
	struct avcs_svc_api_info svc_api_info[];
} __packed;

struct q6core {
	struct apr_device *adev;
	wait_queue_head_t wait;
	uint32_t avcs_state;
	struct mutex lock;
	bool resp_received;
	uint32_t num_services;
	struct avcs_cmdrsp_get_fwk_version *fwk_version;
	struct avcs_cmdrsp_get_version *svc_version;
	bool fwk_version_supported;
	bool get_state_supported;
	bool get_version_supported;
	bool is_version_requested;
};

static struct q6core *g_core;

static int q6core_callback(struct apr_device *adev, const struct apr_resp_pkt *data)
{
	struct q6core *core = dev_get_drvdata(&adev->dev);
	const struct aprv2_ibasic_rsp_result_t *result;
	const struct apr_hdr *hdr = &data->hdr;

	result = data->payload;
	switch (hdr->opcode) {
	case APR_BASIC_RSP_RESULT:{
		result = data->payload;
		switch (result->opcode) {
		case AVCS_GET_VERSIONS:
			if (result->status == ADSP_EUNSUPPORTED)
				core->get_version_supported = false;
			core->resp_received = true;
			break;
		case AVCS_CMD_GET_FWK_VERSION:
			if (result->status == ADSP_EUNSUPPORTED)
				core->fwk_version_supported = false;
			core->resp_received = true;
			break;
		case AVCS_CMD_ADSP_EVENT_GET_STATE:
			if (result->status == ADSP_EUNSUPPORTED)
				core->get_state_supported = false;
			core->resp_received = true;
			break;
		}
		break;
	}
	case AVCS_CMDRSP_GET_FWK_VERSION: {
		struct avcs_cmdrsp_get_fwk_version *fwk;

		fwk = data->payload;

		core->fwk_version = kmemdup(data->payload,
					    struct_size(fwk, svc_api_info,
							fwk->num_services),
					    GFP_ATOMIC);
		if (!core->fwk_version)
			return -ENOMEM;

		core->fwk_version_supported = true;
		core->resp_received = true;

		break;
	}
	case AVCS_GET_VERSIONS_RSP: {
		struct avcs_cmdrsp_get_version *v;

		v = data->payload;

		core->svc_version = kmemdup(data->payload,
					    struct_size(v, svc_api_info,
							v->num_services),
					    GFP_ATOMIC);
		if (!core->svc_version)
			return -ENOMEM;

		core->get_version_supported = true;
		core->resp_received = true;

		break;
	}
	case AVCS_CMDRSP_ADSP_EVENT_GET_STATE:
		core->get_state_supported = true;
		core->avcs_state = result->opcode;

		core->resp_received = true;
		break;
	default:
		dev_err(&adev->dev, "Message id from adsp core svc: 0x%x\n",
			hdr->opcode);
		break;
	}

	if (core->resp_received)
		wake_up(&core->wait);

	return 0;
}

static int q6core_get_fwk_versions(struct q6core *core)
{
	struct apr_device *adev = core->adev;
	struct apr_pkt pkt;
	int rc;

	pkt.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				      APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	pkt.hdr.pkt_size = APR_HDR_SIZE;
	pkt.hdr.opcode = AVCS_CMD_GET_FWK_VERSION;

	rc = apr_send_pkt(adev, &pkt);
	if (rc < 0)
		return rc;

	rc = wait_event_timeout(core->wait, (core->resp_received),
				msecs_to_jiffies(Q6_READY_TIMEOUT_MS));
	if (rc > 0 && core->resp_received) {
		core->resp_received = false;

		if (!core->fwk_version_supported)
			return -ENOTSUPP;
		else
			return 0;
	}


	return rc;
}

static int q6core_get_svc_versions(struct q6core *core)
{
	struct apr_device *adev = core->adev;
	struct apr_pkt pkt;
	int rc;

	pkt.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				      APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	pkt.hdr.pkt_size = APR_HDR_SIZE;
	pkt.hdr.opcode = AVCS_GET_VERSIONS;

	rc = apr_send_pkt(adev, &pkt);
	if (rc < 0)
		return rc;

	rc = wait_event_timeout(core->wait, (core->resp_received),
				msecs_to_jiffies(Q6_READY_TIMEOUT_MS));
	if (rc > 0 && core->resp_received) {
		core->resp_received = false;
		return 0;
	}

	return rc;
}

static bool __q6core_is_adsp_ready(struct q6core *core)
{
	struct apr_device *adev = core->adev;
	struct apr_pkt pkt;
	int rc;

	core->get_state_supported = false;

	pkt.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				      APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	pkt.hdr.pkt_size = APR_HDR_SIZE;
	pkt.hdr.opcode = AVCS_CMD_ADSP_EVENT_GET_STATE;

	rc = apr_send_pkt(adev, &pkt);
	if (rc < 0)
		return false;

	rc = wait_event_timeout(core->wait, (core->resp_received),
				msecs_to_jiffies(Q6_READY_TIMEOUT_MS));
	if (rc > 0 && core->resp_received) {
		core->resp_received = false;

		if (core->avcs_state)
			return true;
	}

	/* assume that the adsp is up if we not support this command */
	if (!core->get_state_supported)
		return true;

	return false;
}

static int q6core_register_topologies(struct device *dev, u32 size, const void *data)
{
	struct q6core *core = dev_get_drvdata(dev);
	struct apr_device *adev = core->adev;
	struct avcs_cmd_register_topologies *cmd;
	struct apr_pkt *pkt;
	void *p, *dest;
	int pkt_size;

	pkt_size = APR_HDR_SIZE + sizeof(*cmd) + size;

	p = kzalloc(pkt_size, GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	pkt = p;
	cmd = p + APR_HDR_SIZE;
	dest = p + APR_HDR_SIZE + sizeof(*cmd);
	if (!p)
		return -ENOMEM;

	pkt->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				      APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	pkt->hdr.pkt_size = APR_HDR_SIZE + sizeof(*cmd) + size;
	pkt->hdr.opcode = AVCS_CMD_REGISTER_TOPOLOGIES;
	cmd->payload_size = size;

	memcpy(dest, data, size);

	return apr_send_pkt(adev, pkt);
}

/**
 * q6core_get_svc_api_info() - Get version number of a service.
 *
 * @svc_id: service id of the service.
 * @ainfo: Valid struct pointer to fill svc api information.
 *
 * Return: zero on success and error code on failure or unsupported
 */
int q6core_get_svc_api_info(int svc_id, struct q6core_svc_api_info *ainfo)
{
	int i;
	int ret = -ENOTSUPP;

	if (!g_core || !ainfo)
		return 0;

	mutex_lock(&g_core->lock);
	if (!g_core->is_version_requested) {
		if (q6core_get_fwk_versions(g_core) == -ENOTSUPP)
			q6core_get_svc_versions(g_core);
		g_core->is_version_requested = true;
	}

	if (g_core->fwk_version_supported) {
		for (i = 0; i < g_core->fwk_version->num_services; i++) {
			struct avcs_svc_api_info *info;

			info = &g_core->fwk_version->svc_api_info[i];
			if (svc_id != info->service_id)
				continue;

			ainfo->api_version = info->api_version;
			ainfo->api_branch_version = info->api_branch_version;
			ret = 0;
			break;
		}
	} else if (g_core->get_version_supported) {
		for (i = 0; i < g_core->svc_version->num_services; i++) {
			struct avcs_svc_info *info;

			info = &g_core->svc_version->svc_api_info[i];
			if (svc_id != info->service_id)
				continue;

			ainfo->api_version = info->version;
			ainfo->api_branch_version = 0;
			ret = 0;
			break;
		}
	}

	mutex_unlock(&g_core->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(q6core_get_svc_api_info);

/**
 * q6core_is_adsp_ready() - Get status of adsp
 *
 * Return: Will be an true if adsp is ready and false if not.
 */
bool q6core_is_adsp_ready(void)
{
	unsigned long  timeout;
	bool ret = false;

	if (!g_core)
		return false;

	mutex_lock(&g_core->lock);
	timeout = jiffies + msecs_to_jiffies(ADSP_STATE_READY_TIMEOUT_MS);
	for (;;) {
		if (__q6core_is_adsp_ready(g_core)) {
			ret = true;
			break;
		}

		if (!time_after(timeout, jiffies)) {
			ret = false;
			break;
		}
	}

	mutex_unlock(&g_core->lock);
	return ret;
}
EXPORT_SYMBOL_GPL(q6core_is_adsp_ready);

static int acdb_section_offset(const struct firmware *fw,
			       u64 section, u32 *off_out)
{
	const u8 *data = fw->data + 32;
	u32 off = 32;
	u32 sect_size;
	u64 sect_id;

	while (off + 12 <= fw->size) {
		sect_id = *(const u64 *) data;
		sect_size = *(const u32 *) (data + 8);

		if (off + 12 + sect_size > fw->size)
			return -EINVAL;

		if (sect_id == section) {
			*off_out = off;
			return 0;
		}

		off += 12 + sect_size;
		data += 12 + sect_size;
	}

	return -ENOENT;
}

static int acdb_get_global_property(const struct firmware *fw,
				    u32 prop_id, u32 *offset)
{
	u32 gproplut_off, datapool_off;
	u32 lut_size, lut_nelts, i;
	const u32 *data;
	int ret;

	ret = acdb_section_offset(fw, ACDB_SECTION_GPROPLUT, &gproplut_off);
	if (ret)
		return ret;

	ret = acdb_section_offset(fw, ACDB_SECTION_DATAPOOL, &datapool_off);
	if (ret)
		return ret;

	lut_size = *(const u32 *) (fw->data + gproplut_off + 8);
	if (lut_size < 4)
		return -EINVAL;

	lut_nelts = *(const u32 *) (fw->data + gproplut_off + 12);
	if (lut_size < 4 + 8 * lut_nelts)
		return -EINVAL;

	data = (const u32 *) (fw->data + gproplut_off + 16);
	for (i = 0; i < lut_nelts; i++) {
		if (datapool_off + 12 + data[i * 2 + 1] + 4 > fw->size)
			return -EINVAL;

		if (data[i * 2] == prop_id) {
			*offset = datapool_off + 12 + data[i * 2 + 1];
			return 0;
		}
	}

	return -ENOENT;
}

static int q6core_load_and_register_topologies(struct device *dev)
{
	const struct device_node *np = dev_of_node(dev);
	const char *fw_name;
	const struct firmware *fw;
	u32 topo_off, size;
	int ret;

	ret = of_property_read_string(np, "qcom,acdb-name", &fw_name);
	if (ret)
		return 0;

	ret = request_firmware(&fw, fw_name, dev);
	if (ret)
		return dev_err_probe(dev, ret, "failed to load Global_cal.acdb");

	ret = acdb_get_global_property(fw, ACDB_TOPOLOGIES_BLOB, &topo_off);
	if (ret) {
		dev_err(dev, "ACDB parse error: %d\n", ret);
		goto release_fw;
	}

	size = *(const u32 *) (fw->data + topo_off);
	ret = q6core_register_topologies(dev, size, fw->data + topo_off + 4);
	if (ret < 0)
		goto release_fw;

release_fw:
	release_firmware(fw);
	return 0;
}

static int q6core_probe(struct apr_device *adev)
{
	g_core = kzalloc_obj(*g_core);
	if (!g_core)
		return -ENOMEM;

	dev_set_drvdata(&adev->dev, g_core);

	mutex_init(&g_core->lock);
	g_core->adev = adev;
	init_waitqueue_head(&g_core->wait);

	return q6core_load_and_register_topologies(&adev->dev);
}

static void q6core_exit(struct apr_device *adev)
{
	struct q6core *core = dev_get_drvdata(&adev->dev);

	if (core->fwk_version_supported)
		kfree(core->fwk_version);
	if (core->get_version_supported)
		kfree(core->svc_version);

	g_core = NULL;
	kfree(core);
}

#ifdef CONFIG_OF
static const struct of_device_id q6core_device_id[]  = {
	{ .compatible = "qcom,q6core" },
	{},
};
MODULE_DEVICE_TABLE(of, q6core_device_id);
#endif

static struct apr_driver qcom_q6core_driver = {
	.probe = q6core_probe,
	.remove = q6core_exit,
	.callback = q6core_callback,
	.driver = {
		.name = "qcom-q6core",
		.of_match_table = of_match_ptr(q6core_device_id),
	},
};

module_apr_driver(qcom_q6core_driver);
MODULE_DESCRIPTION("q6 core");
MODULE_LICENSE("GPL v2");
