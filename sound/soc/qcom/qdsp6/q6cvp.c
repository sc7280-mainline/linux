// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
// Copyright (c) 2020, Stephan Gerhold

#include <linux/module.h>
#include <linux/of.h>
#include <linux/soc/qcom/apr.h>
#include "q6cvp.h"
#include "q6dsp-common.h"
#include "q6voice-common.h"

#define VSS_IVOCPROC_DIRECTION_RX	0
#define VSS_IVOCPROC_DIRECTION_TX	1
#define VSS_IVOCPROC_DIRECTION_RX_TX	2

#define VSS_IVOCPROC_PORT_ID_NONE	0xFFFF

#define VSS_IVOCPROC_TOPOLOGY_ID_NONE			0x00010F70
#define VSS_IVOCPROC_TOPOLOGY_ID_TX_SM_ECNS		0x00010F71
#define VSS_IVOCPROC_TOPOLOGY_ID_TX_DM_FLUENCE		0x00010F72

#define VSS_IVOCPROC_TOPOLOGY_ID_RX_DEFAULT		0x00010F77

#define VSS_IVOCPROC_VOCPROC_MODE_EC_INT_MIXING		0x00010F7C
#define VSS_IVOCPROC_VOCPROC_MODE_EC_EXT_MIXING		0x00010F7D

#define VSS_ICOMMON_CAL_NETWORK_ID_NONE			0x0001135E

#define VSS_IVOCPROC_CMD_ENABLE				0x000100C6
#define VSS_IVOCPROC_CMD_DISABLE			0x000110E1

#define VSS_IVOCPROC_CMD_CREATE_FULL_CONTROL_SESSION_V2	0x000112BF
#define VSS_IVOCPROC_CMD_CREATE_FULL_CONTROL_SESSION_V3	0x00013169

#define VSS_IVOCPROC_CMD_TOPOLOGY_COMMIT		0x00013198

#define VSS_ICOMMON_CMD_SET_PARAM_V2			0x0001133D

#define VSS_MODULE_CVD_GENERIC				0x0001316E

#define VSS_PARAM_VOCPROC_RX_CHANNEL_INFO		0x0001328F
#define VSS_PARAM_VOCPROC_TX_CHANNEL_INFO		0x0001328E
#define VSS_PARAM_RX_PORT_ENDPOINT_MEDIA_INFO		0x00013254
#define VSS_PARAM_TX_PORT_ENDPOINT_MEDIA_INFO		0x00013253

#define VOC_SET_MEDIA_FORMAT_PARAM_TOKEN		2

#define VSS_NUM_CHANNELS_MAX 8

struct vss_ivocproc_cmd_create_full_control_session_v2_cmd {
	struct apr_hdr hdr;

	/*
	 * Vocproc direction. The supported values:
	 * VSS_IVOCPROC_DIRECTION_RX
	 * VSS_IVOCPROC_DIRECTION_TX
	 * VSS_IVOCPROC_DIRECTION_RX_TX
	 */
	u16 direction;

	/*
	 * Tx device port ID to which the vocproc connects. If a port ID is
	 * not being supplied, set this to #VSS_IVOCPROC_PORT_ID_NONE.
	 */
	u16 tx_port_id;

	/*
	 * Tx path topology ID. If a topology ID is not being supplied, set
	 * this to #VSS_IVOCPROC_TOPOLOGY_ID_NONE.
	 */
	u32 tx_topology_id;

	/*
	 * Rx device port ID to which the vocproc connects. If a port ID is
	 * not being supplied, set this to #VSS_IVOCPROC_PORT_ID_NONE.
	 */
	u16 rx_port_id;

	/*
	 * Rx path topology ID. If a topology ID is not being supplied, set
	 * this to #VSS_IVOCPROC_TOPOLOGY_ID_NONE.
	 */
	u32 rx_topology_id;

	/* Voice calibration profile ID. */
	u32 profile_id;

	/*
	 * Vocproc mode. The supported values:
	 * VSS_IVOCPROC_VOCPROC_MODE_EC_INT_MIXING
	 * VSS_IVOCPROC_VOCPROC_MODE_EC_EXT_MIXING
	 */
	u32 vocproc_mode;

	/*
	 * Port ID to which the vocproc connects for receiving echo
	 * cancellation reference signal. If a port ID is not being supplied,
	 * set this to #VSS_IVOCPROC_PORT_ID_NONE. This parameter value is
	 * ignored when the vocproc_mode parameter is set to
	 * VSS_IVOCPROC_VOCPROC_MODE_EC_INT_MIXING.
	 */
	u16 ec_ref_port_id;

	/*
	 * Session name string used to identify a session that can be shared
	 * with passive controllers (optional).
	 */
	char name[20];
} __packed;

struct vss_param_endpoint_media_format_info {
	u32 port_id;
	u16 num_channels;
	u16 bits_per_sample;
	u32 sample_rate;
	u8 channel_mapping[VSS_NUM_CHANNELS_MAX];
} __packed;

struct vss_param_vocproc_dev_channel_info {
	u32 num_channels;
	u32 bits_per_sample;
	u8 channel_mapping[VSS_NUM_CHANNELS_MAX];
} __packed;

struct vss_icommon_param_data {
	u32 module_id;
	u32 param_id;
	u16 param_size;
	u16 reserved;

	union {
		struct vss_param_endpoint_media_format_info media_format_info;
		struct vss_param_vocproc_dev_channel_info channel_info;
	};
} __packed;

struct vss_icommon_cmd_set_param_v2_cmd {
	struct apr_hdr hdr;

	/*
	 * Pointer to the unique identifier for an address. What type of
	 * address? DMA? Device-specific memory? Please direct inquiries of
	 * this type to the SoC manufacturer.
	 *
	 * Anyway, the most important information for this parameter is that
	 * this field is set to zero if the data is in-band, i.e. in the same
	 * packet.
	 */
	u32 mem_handle;

	/*
	 * This field is ignored if you are only paying attention to this
	 * documentation of the fields. Otherwise, refer to your own
	 * documentation. I did not ask for this, and will not fully document
	 * it if its accuracy cannot be measured.
	 */
	u64 mem_address;

	/*
	 * Just set this to the size of the parameter data.
	 */
	u32 mem_size;

	struct vss_icommon_param_data param_data;
} __packed;

struct q6voice_session *q6cvp_session_create(enum q6voice_path_type path,
					     u16 tx_port, u16 rx_port,
					     u32 tx_topo, u32 rx_topo)
{
	struct vss_ivocproc_cmd_create_full_control_session_v2_cmd cmd;

	cmd.hdr.pkt_size = sizeof(cmd);
	cmd.hdr.opcode = VSS_IVOCPROC_CMD_CREATE_FULL_CONTROL_SESSION_V2;

	cmd.tx_topology_id = tx_topo;
	cmd.rx_topology_id = rx_topo;

	cmd.direction = VSS_IVOCPROC_DIRECTION_RX_TX;
	cmd.tx_port_id = tx_port;
	cmd.rx_port_id = rx_port;
	cmd.profile_id = VSS_ICOMMON_CAL_NETWORK_ID_NONE;
	cmd.vocproc_mode = VSS_IVOCPROC_VOCPROC_MODE_EC_INT_MIXING;
	cmd.ec_ref_port_id = VSS_IVOCPROC_PORT_ID_NONE;

	return q6voice_session_create(Q6VOICE_SERVICE_CVP, path, &cmd.hdr);
}
EXPORT_SYMBOL_GPL(q6cvp_session_create);

struct q6voice_session *q6cvp_session_create_v3(enum q6voice_path_type path,
						u16 tx_port, u16 rx_port,
						u32 tx_topo, u32 rx_topo)
{
	/*
	 * According to downstream, the v3 parameters are the exact same, with
	 * the only difference being the opcode.
	 */
	struct vss_ivocproc_cmd_create_full_control_session_v2_cmd cmd;

	cmd.hdr.pkt_size = sizeof(cmd);
	cmd.hdr.opcode = VSS_IVOCPROC_CMD_CREATE_FULL_CONTROL_SESSION_V3;

	cmd.tx_topology_id = tx_topo;
	cmd.rx_topology_id = rx_topo;

	cmd.direction = VSS_IVOCPROC_DIRECTION_RX_TX;
	cmd.tx_port_id = tx_port;
	cmd.rx_port_id = rx_port;
	cmd.profile_id = VSS_ICOMMON_CAL_NETWORK_ID_NONE;
	cmd.vocproc_mode = VSS_IVOCPROC_VOCPROC_MODE_EC_INT_MIXING;
	cmd.ec_ref_port_id = VSS_IVOCPROC_PORT_ID_NONE;

	return q6voice_session_create(Q6VOICE_SERVICE_CVP, path, &cmd.hdr);
}
EXPORT_SYMBOL_GPL(q6cvp_session_create_v3);

int q6cvp_send_channel_info(struct q6voice_session *cvp, bool is_tx)
{
	struct vss_icommon_cmd_set_param_v2_cmd cmd;

	cmd.hdr.pkt_size = offsetof(struct vss_icommon_cmd_set_param_v2_cmd, param_data.channel_info)
			 + sizeof(struct vss_param_vocproc_dev_channel_info);
	cmd.hdr.opcode = VSS_ICOMMON_CMD_SET_PARAM_V2;

	cmd.mem_handle = 0;
	cmd.mem_size = offsetof(struct vss_icommon_param_data, channel_info)
		     + sizeof(struct vss_param_vocproc_dev_channel_info);

	cmd.param_data.module_id = VSS_MODULE_CVD_GENERIC;
	cmd.param_data.param_id = is_tx ? VSS_PARAM_VOCPROC_TX_CHANNEL_INFO
					: VSS_PARAM_VOCPROC_RX_CHANNEL_INFO;
	cmd.param_data.param_size = sizeof(struct vss_param_vocproc_dev_channel_info);
	cmd.param_data.reserved = 0;

	if (is_tx)
		cmd.param_data.channel_info.num_channels = 1;
	else
		cmd.param_data.channel_info.num_channels = 2;

	cmd.param_data.channel_info.bits_per_sample = 16;

	q6dsp_map_channels(cmd.param_data.channel_info.channel_mapping,
			   cmd.param_data.channel_info.num_channels);

	return q6voice_common_send(cvp, &cmd.hdr);
}
EXPORT_SYMBOL_GPL(q6cvp_send_channel_info);

int q6cvp_send_media_format(struct q6voice_session *cvp, int port_id, bool is_tx)
{
	struct vss_icommon_cmd_set_param_v2_cmd cmd;

	cmd.hdr.pkt_size = offsetof(struct vss_icommon_cmd_set_param_v2_cmd, param_data.media_format_info)
			 + sizeof(struct vss_param_endpoint_media_format_info);
	cmd.hdr.opcode = VSS_ICOMMON_CMD_SET_PARAM_V2;

	cmd.mem_handle = 0;
	cmd.mem_size = offsetof(struct vss_icommon_param_data, media_format_info)
		     + sizeof(struct vss_param_endpoint_media_format_info);

	cmd.param_data.module_id = VSS_MODULE_CVD_GENERIC;
	cmd.param_data.param_id = is_tx ? VSS_PARAM_TX_PORT_ENDPOINT_MEDIA_INFO
					: VSS_PARAM_RX_PORT_ENDPOINT_MEDIA_INFO;
	cmd.param_data.param_size = sizeof(struct vss_param_endpoint_media_format_info);
	cmd.param_data.reserved = 0;

	cmd.param_data.media_format_info.port_id = port_id;

	if (is_tx)
		cmd.param_data.media_format_info.num_channels = 1;
	else
		cmd.param_data.media_format_info.num_channels = 2;

	cmd.param_data.media_format_info.bits_per_sample = 16;
	cmd.param_data.media_format_info.sample_rate = 48000;

	q6dsp_map_channels(cmd.param_data.channel_info.channel_mapping,
			   cmd.param_data.channel_info.num_channels);

	return q6voice_common_send(cvp, &cmd.hdr);
}
EXPORT_SYMBOL_GPL(q6cvp_send_media_format);

int q6cvp_topology_commit(struct q6voice_session *cvp)
{
	struct apr_pkt cmd;

	cmd.hdr.pkt_size = APR_HDR_SIZE;
	cmd.hdr.opcode = VSS_IVOCPROC_CMD_TOPOLOGY_COMMIT;

	return q6voice_common_send(cvp, &cmd.hdr);
}
EXPORT_SYMBOL_GPL(q6cvp_topology_commit);

int q6cvp_enable(struct q6voice_session *cvp, bool state)
{
	struct apr_pkt cmd;

	cmd.hdr.pkt_size = APR_HDR_SIZE;
	cmd.hdr.opcode = state ? VSS_IVOCPROC_CMD_ENABLE : VSS_IVOCPROC_CMD_DISABLE;

	return q6voice_common_send(cvp, &cmd.hdr);
}
EXPORT_SYMBOL_GPL(q6cvp_enable);

static int q6cvp_probe(struct apr_device *adev)
{
	return q6voice_common_probe(adev, Q6VOICE_SERVICE_CVP);
}

static const struct of_device_id q6cvp_device_id[]  = {
	{ .compatible = "qcom,q6cvp" },
	{},
};
MODULE_DEVICE_TABLE(of, q6cvp_device_id);

static struct apr_driver qcom_q6cvp_driver = {
	.probe = q6cvp_probe,
	.remove = q6voice_common_remove,
	.callback = q6voice_common_callback,
	.driver = {
		.name = "qcom-q6cvp",
		.of_match_table = of_match_ptr(q6cvp_device_id),
	},
};

module_apr_driver(qcom_q6cvp_driver);

MODULE_AUTHOR("Stephan Gerhold <stephan@gerhold.net>");
MODULE_DESCRIPTION("Q6 Core Voice Processor");
MODULE_LICENSE("GPL v2");
