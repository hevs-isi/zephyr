//------------------------------------------------------------------------------
//
//	File:		wimod_lorawan_API.cpp
//
//	Abstract:	API Layer of LoRaWAN Host Controller Interface
//
//	Version:	0.1
//
//	Date:		18.05.2016
//
//	Disclaimer:	This example code is provided by IMST GmbH on an "AS IS" basis
//				without any warranties.
//
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//
// Include Files
//
//------------------------------------------------------------------------------

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "wimod_hci_driver.h"
#include "wimod_lorawan_api.h"
#include "misc/byteorder.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(wimod_lorawan_api, LOG_LEVEL_DBG);

#define MAKEWORD(lo,hi) ((lo)|((hi) << 8))
#define MAKELONG(lo,hi) ((lo)|((hi) << 16))

static uint32_t mk_uint32_t(const uint8_t *buffer)
{
	uint32_t tmp;
	memcpy(&tmp, buffer, sizeof(tmp));
	return sys_le32_to_cpu(tmp);
}

//------------------------------------------------------------------------------
//
//  Forward Declarations
//
//------------------------------------------------------------------------------

// HCI Message Receiver callback
static wimod_hci_message_t*
wimod_lorawan_process_rx_msg(wimod_hci_message_t* rx_msg);

static void
wimod_lorawan_process_devmgmt_message(wimod_hci_message_t* rx_msg);

static void
wimod_lorawan_devmgmt_device_info_rsp(wimod_hci_message_t* rx_msg);

static void
wimod_lorawan_devmgmt_firmware_version_rsp(wimod_hci_message_t* rx_msg);

static void
wimod_lorawan_get_opmode_rsp(wimod_hci_message_t* rx_msg);

static void
wimod_lorawan_get_rtc_rsp(wimod_hci_message_t* rx_msg);

static void
wimod_lorawan_get_rtc_alarm_rsp(wimod_hci_message_t* rx_msg);

static void
wimod_lorawan_process_lorawan_message(wimod_hci_message_t* rx_msg);

static void
wimod_lorawan_process_join_tx_indication(wimod_hci_message_t* rx_msg);

static void
wimod_lorawan_process_join_network_indication(wimod_hci_message_t* rx_msg);

static void
wimod_lorawan_process_u_data_rx_indication(wimod_hci_message_t* rx_msg);

static void
wimod_lorawan_process_c_data_rx_indication(wimod_hci_message_t* rx_msg);

static void
wimod_lorawan_show_response(const char* string, const id_string_t* status_table, u8_t status_id);

static void
wimod_lorawan_show_response_tx(const char* string, const id_string_t* status_table, u8_t status_id);

//------------------------------------------------------------------------------
//
//  Section RAM
//
//------------------------------------------------------------------------------

// reserve one Tx-Message
wimod_hci_message_t tx_message;

// reserve one Rx-Message
wimod_hci_message_t rx_message;

// network join callback function
static join_network_cb join_callback;


//------------------------------------------------------------------------------
//
//  Section Const
//
//------------------------------------------------------------------------------

// Status Codes for DeviceMgmt Response Messages
static const id_string_t wimod_device_mgmt_status_strings[] =
{
	{ DEVMGMT_STATUS_OK,				"ok" },
	{ DEVMGMT_STATUS_ERROR,				"error" } ,
	{ DEVMGMT_STATUS_CMD_NOT_SUPPORTED,	"command not supported" },
	{ DEVMGMT_STATUS_WRONG_PARAMETER,	"wrong parameter" },
	{ DEVMGMT_STATUS_WRONG_DEVICE_MODE,	"wrong device mode" },

	// end of table
	{ 0, 0 }
};

// Status Codes for LoRaWAN Response Messages
static const id_string_t wimod_lorawan_status_strings[] =
{
	{ LORAWAN_STATUS_OK,					"ok" },
	{ LORAWAN_STATUS_ERROR,				"error" } ,
	{ LORAWAN_STATUS_CMD_NOT_SUPPORTED,	"command not supported" },
	{ LORAWAN_STATUS_WRONG_PARAMETER,	"wrong parameter" },
	{ LORAWAN_STATUS_WRONG_DEVICE_MODE,	"wrong device mode" },
	{ LORAWAN_STATUS_NOT_ACTIVATED,		"device not activated" },
	{ LORAWAN_STATUS_BUSY,				"device busy - command rejected" },
	{ LORAWAN_STATUS_QUEUE_FULL,			"message queue full - command rejected" },
	{ LORAWAN_STATUS_LENGTH_ERROR,		"HCI message length error" },
	{ LORAWAN_STATUS_NO_FACTORY_SETTINGS,   "no factory settings available" },
	{ LORAWAN_STATUS_CHANNEL_BLOCKED_BY_DC, "error: channel blocked due to duty cycle, please try later again" },
	{ LORAWAN_STATUS_CHANNEL_NOT_AVAILABLE, "error: channel not available" },

	// end of table
	{ 0, 0 }
};
//------------------------------------------------------------------------------
//
//  Section Code
//
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//
//  Init
//
//  @brief: init complete interface
//
//------------------------------------------------------------------------------

static K_MUTEX_DEFINE(wimod_mutex);

int wimod_lorawan_init()
{
	// init HCI layer
	int status;

	k_mutex_lock(&wimod_mutex, K_FOREVER);

	status = wimod_hci_init(wimod_lorawan_process_rx_msg, &rx_message);

	k_mutex_unlock(&wimod_mutex);

	return status;
}

static void wimod_timer_stop_handler(struct k_timer *timer_id)
{
	LOG_DBG("stopped");
}

static void wimod_timer_expired_handler(struct k_timer *timer_id)
{
	LOG_WRN("expired");
}

K_TIMER_DEFINE(wimod_timer, wimod_timer_expired_handler, wimod_timer_stop_handler);

static int wimod_send_message_block(wimod_hci_message_t* tx_message)
{
	int status;

	k_mutex_lock(&wimod_mutex, K_FOREVER);

	k_timer_user_data_set(&wimod_timer, tx_message);
	k_timer_start(&wimod_timer, K_SECONDS(1), 0);
	LOG_DBG("sap_id:0x%02"PRIx8",msg_id:0x%02"PRIx8, tx_message->sap_id, tx_message->msg_id);
	status = wimod_hci_send_message(tx_message);
	if (status != 0)
	{
		LOG_ERR("failed");
		k_timer_stop(&wimod_timer);
		k_mutex_unlock(&wimod_mutex);
		return -1;
	}

	uint32_t timer_status = k_timer_status_sync(&wimod_timer);

	k_mutex_unlock(&wimod_mutex);

	return timer_status == 0 ? 0 : -1;
}

int wimod_lorawan_reset()
{
	int status;
	tx_message.sap_id = DEVMGMT_SAP_ID;
	tx_message.msg_id = DEVMGMT_MSG_RESET_REQ;
	tx_message.length = 0;

	status = wimod_send_message_block(&tx_message);
	if (status)
	{
		return status;
	}

	k_sleep(100);

	return wimod_lorawan_send_ping();
}

int wimod_lorawan_factory_reset()
{
	// 1. init header
	tx_message.sap_id = LORAWAN_SAP_ID;
	tx_message.msg_id = LORAWAN_MSG_FACTORY_RESET_REQ;
	tx_message.length = 0;

	// 2. send HCI message without payload
	return wimod_send_message_block(&tx_message);
}

//------------------------------------------------------------------------------
//
//  Ping
//
//  @brief: ping device
//
//------------------------------------------------------------------------------

int wimod_lorawan_send_ping()
{
	// 1. init header
	tx_message.sap_id = DEVMGMT_SAP_ID;
	tx_message.msg_id = DEVMGMT_MSG_PING_REQ;
	tx_message.length = 0;

	// 2. send HCI message without payload
	return wimod_send_message_block(&tx_message);
}

int wimod_lorawan_get_device_info()
{
	// 1. init header
	tx_message.sap_id = DEVMGMT_SAP_ID;
	tx_message.msg_id = DEVMGMT_MSG_GET_DEVICE_INFO_REQ;
	tx_message.length = 0;

	// 2. send HCI message without payload
	return wimod_send_message_block(&tx_message);
}

//------------------------------------------------------------------------------
//
//  GetFirmwareVersion
//
//  @brief: get firmware version
//
//------------------------------------------------------------------------------

int wimod_lorawan_get_firmware_version()
{
	// 1. init header
	tx_message.sap_id = DEVMGMT_SAP_ID;
	tx_message.msg_id = DEVMGMT_MSG_GET_FW_VERSION_REQ;
	tx_message.length = 0;

	// 2. send HCI message without payload
	return wimod_send_message_block(&tx_message);
}

int wimod_lorawan_set_op_mode()
{
	// 1. init header
	tx_message.sap_id = DEVMGMT_SAP_ID;
	tx_message.msg_id = DEVMGMT_MSG_SET_OPMODE_REQ ;
	tx_message.length = 1;
	tx_message.payload[0] = LORAWAN_OPERATION_MODE_CUSTOMER;

	// 2. send HCI message without payload
	return wimod_send_message_block(&tx_message);
}

int wimod_lorawan_get_op_mode()
{
	// 1. init header
	tx_message.sap_id = DEVMGMT_SAP_ID;
	tx_message.msg_id = DEVMGMT_MSG_GET_OPMODE_REQ  ;
	tx_message.length = 0;

	// 2. send HCI message without payload
	return wimod_send_message_block(&tx_message);
}

int wimod_lorawan_get_rtc()
{
	// 1. init header
	tx_message.sap_id = DEVMGMT_SAP_ID;
	tx_message.msg_id = DEVMGMT_MSG_GET_RTC_REQ;
	tx_message.length = 0;

	// 2. send HCI message without payload
	return wimod_send_message_block(&tx_message);
}

int wimod_lorawan_set_rtc()
{
	// 1. init header
	tx_message.sap_id = DEVMGMT_SAP_ID;
	tx_message.msg_id = DEVMGMT_MSG_SET_RTC_REQ;
	tx_message.length = 4;

	tx_message.payload[0] = 0;
	tx_message.payload[1] = 0;
	tx_message.payload[2] = 0;
	tx_message.payload[3] = 0;

	// 2. send HCI message without payload
	return wimod_send_message_block(&tx_message);
}

int wimod_lorawan_get_rtc_alarm()
{
	// 1. init header
	tx_message.sap_id = DEVMGMT_SAP_ID;
	tx_message.msg_id = DEVMGMT_MSG_GET_RTC_ALARM_REQ;
	tx_message.length = 0;

	// 2. send HCI message without payload
	return wimod_send_message_block(&tx_message);
}

int wimod_lorawan_get_device_eui()
{
	// 1. init header
	tx_message.sap_id = LORAWAN_SAP_ID;
	tx_message.msg_id = LORAWAN_MSG_GET_DEVICE_EUI_REQ;
	tx_message.length = 0;

	// 2. send HCI message without payload
	return wimod_send_message_block(&tx_message);
}


int wimod_lorawan_set_join_param_request(const char *appEui, const char *appKey)
{
	const char *pos;
	size_t i;
	char buf[5];

	// 1. init header
	tx_message.sap_id = LORAWAN_SAP_ID;
	tx_message.msg_id = LORAWAN_MSG_SET_JOIN_PARAM_REQ;
	tx_message.length = 24;

	pos = appEui;
	for (i = 0; i < 8; i++) {
		sprintf(buf, "0x%c%c", pos[0+2*i], pos[1+2*i]);
		tx_message.payload[i] = strtol(buf, NULL, 0);
	}

	pos = appKey;
	for (i = 0; i < 16; i++) {
		sprintf(buf, "0x%c%c", pos[0+2*i], pos[1+2*i]);
		tx_message.payload[8+i] = strtol(buf, NULL, 0);
	}

	// 2. send HCI message with payload
	return wimod_send_message_block(&tx_message);
}

int wimod_lorawan_reactivate(void)
{
	tx_message.sap_id = LORAWAN_SAP_ID;
	tx_message.msg_id = LORAWAN_MSG_REACTIVATE_DEVICE_REQ;
	tx_message.length = 0;

	return wimod_send_message_block(&tx_message);
}

//------------------------------------------------------------------------------
//
//  JoinNetworkRequest
//
//  @brief: send join radio message
//
//------------------------------------------------------------------------------

int wimod_lorawan_join_network_request(join_network_cb cb)
{
	// 1. init header
	tx_message.sap_id = LORAWAN_SAP_ID;
	tx_message.msg_id = LORAWAN_MSG_JOIN_NETWORK_REQ;
	tx_message.length = 0;

	join_callback = cb;

	// 2. send HCI message with payload
	return wimod_send_message_block(&tx_message);
}

int wimod_lorawan_get_nwk_status()
{
	// 1. init header
	tx_message.sap_id = LORAWAN_SAP_ID;
	tx_message.msg_id = LORAWAN_MSG_GET_NWK_STATUS_REQ;
	tx_message.length = 0;

	// 2. send HCI message without payload
	return wimod_send_message_block(&tx_message);
}

int wimod_lorawan_get_rstack_config()
{
	// 1. init header
	tx_message.sap_id = LORAWAN_SAP_ID;
	tx_message.msg_id = LORAWAN_MSG_GET_RSTACK_CONFIG_REQ;
	tx_message.length = 0;

	// 2. send HCI message without payload
	return wimod_send_message_block(&tx_message);
}

int wimod_lorawan_set_rstack_config()
{
	// 1. init header
	tx_message.sap_id = LORAWAN_SAP_ID;
	tx_message.msg_id = LORAWAN_MSG_SET_RSTACK_CONFIG_REQ;
	tx_message.length = 6;

	tx_message.payload[0] = 0;
	tx_message.payload[1] = 16;
	tx_message.payload[2] = 0x01;
	tx_message.payload[3] = 0x01;
	tx_message.payload[4] = 7;
	tx_message.payload[5] = 1;
	tx_message.payload[6] = 15;

	// 2. send HCI message without payload
	return wimod_send_message_block(&tx_message);
}

//------------------------------------------------------------------------------
//
//  SendURadioData
//
//  @brief: send unconfirmed radio message
//
//------------------------------------------------------------------------------

int wimod_lorawan_send_u_radio_data(u8_t port, const void* _src_data, int src_length)
{
	const u8_t* src_data = (const u8_t*)_src_data;
	// 1. check length
	if (src_length > (WIMOD_HCI_MSG_PAYLOAD_SIZE - 1))
	{
		// error
		return -1;
	}

	// 2. init header
	tx_message.sap_id = LORAWAN_SAP_ID;
	tx_message.msg_id = LORAWAN_MSG_SEND_UDATA_REQ;
	tx_message.length = 1 + src_length;

	// 3.  init payload
	// 3.1 init port
	tx_message.payload[0] = port;

	// 3.2 init radio message payload
	memcpy(&tx_message.payload[1], src_data, src_length);

	// 4. send HCI message with payload
	return wimod_send_message_block(&tx_message);
}

//------------------------------------------------------------------------------
//
//  SendCRadioData
//
//  @brief: send confirmed radio message
//
//------------------------------------------------------------------------------

int wimod_lorawan_send_c_radio_data(u8_t port, const void* _src_data, int src_length)
{
	const u8_t* src_data = (const u8_t*)_src_data;

	// 1. check length
	if (src_length > (WIMOD_HCI_MSG_PAYLOAD_SIZE - 1))
	{
		// error
		return -1;
	}

	// 2. init header
	tx_message.sap_id = LORAWAN_SAP_ID;
	tx_message.msg_id = LORAWAN_MSG_SEND_CDATA_REQ;
	tx_message.length = 1 + src_length;

	// 3.  init payload
	// 3.1 init port
	tx_message.payload[0] = port;

	// 3.2 init radio message payload
	memcpy(&tx_message.payload[1], src_data, src_length);

	// 4. send HCI message with payload
	return wimod_send_message_block(&tx_message);
}

//------------------------------------------------------------------------------
//
//  Process
//
//  @brief: handle receiver process
//
//------------------------------------------------------------------------------

void wimod_lorawan_process()
{
	// call HCI process
	wimod_hci_process();
}

//------------------------------------------------------------------------------
//
//  Process
//
//  @brief: handle receiver process
//
//------------------------------------------------------------------------------

static wimod_hci_message_t* wimod_lorawan_process_rx_msg(wimod_hci_message_t* rx_msg)
{
	if (k_timer_remaining_get(&wimod_timer))
	{
		/*
		 * IFF the timer is running, the wimod_send_message_block is still
		 * waiting, and the user data is still valid (on the stack.)
		 */
		const wimod_hci_message_t *tx_msg = (wimod_hci_message_t *)k_timer_user_data_get(&wimod_timer);

		/*
		 * Stop the timer only if this is a the _RSP according to the last _REQ.
		 * According to the spec, the _RSP is always _REQ+1.
		 * The _RSP should have the same SAP_ID.
		 */
		LOG_DBG("tx:sap_id:0x%02"PRIx8",msg_id:0x%02"PRIx8, tx_msg->sap_id, tx_msg->msg_id);
		LOG_DBG("rx:sap_id:0x%02"PRIx8",msg_id:0x%02"PRIx8, rx_msg->sap_id, rx_msg->msg_id);

		if ((tx_msg->sap_id == rx_msg->sap_id) && (tx_msg->msg_id+1 == rx_msg->msg_id))
		{
			k_timer_stop(&wimod_timer);
		}
	}

	LOG_DBG("here");
	switch(rx_msg->sap_id)
	{
		case DEVMGMT_SAP_ID:
			wimod_lorawan_process_devmgmt_message(rx_msg);
			break;


		case LORAWAN_SAP_ID:
			wimod_lorawan_process_lorawan_message(rx_msg);
			break;
	}
	return &rx_message;
}

//------------------------------------------------------------------------------
//
//  Process_DevMgmt_Message
//
//  @brief: handle received Device Management SAP messages
//
//------------------------------------------------------------------------------

static void wimod_lorawan_process_devmgmt_message(wimod_hci_message_t* rx_msg)
{
	LOG_DBG("here");
	switch(rx_msg->msg_id)
	{
		case	DEVMGMT_MSG_RESET_RSP:
				LOG_DBG("device reset");
		break;
				case	DEVMGMT_MSG_PING_RSP:
				wimod_lorawan_show_response("ping response", wimod_device_mgmt_status_strings, rx_msg->payload[0]);
				break;

		case	DEVMGMT_MSG_GET_DEVICE_INFO_RSP:
				wimod_lorawan_devmgmt_device_info_rsp(rx_msg);
				break;


		case	DEVMGMT_MSG_GET_FW_VERSION_RSP:
				wimod_lorawan_devmgmt_firmware_version_rsp(rx_msg);
				break;

		case	DEVMGMT_MSG_SET_OPMODE_RSP:
				wimod_lorawan_show_response("set opmode response", wimod_device_mgmt_status_strings, rx_msg->payload[0]);
				break;

		case	DEVMGMT_MSG_GET_OPMODE_RSP:
				wimod_lorawan_get_opmode_rsp(rx_msg);
				break;

		case	DEVMGMT_MSG_GET_RTC_RSP:
				wimod_lorawan_get_rtc_rsp(rx_msg);
				break;

		case	DEVMGMT_MSG_SET_RTC_RSP:
				wimod_lorawan_show_response("set rtc response", wimod_device_mgmt_status_strings, rx_msg->payload[0]);
				break;

		case	DEVMGMT_MSG_GET_RTC_ALARM_RSP:
				wimod_lorawan_get_rtc_alarm_rsp(rx_msg);
				break;

		default:
				LOG_DBG("unhandled DeviceMgmt message received - msg_id : 0x%02X", (u8_t)rx_msg->msg_id);
				break;
	}
}

static void wimod_lorawan_devmgmt_device_info_rsp(wimod_hci_message_t* rx_msg)
{
	wimod_lorawan_show_response("device info response", wimod_device_mgmt_status_strings, rx_msg->payload[0]);

	if (rx_msg->payload[0] == DEVMGMT_STATUS_OK)
	{
		struct lw_dev_info_t info =
		{
			.type = rx_msg->payload[1],
			.addr = mk_uint32_t(&rx_msg->payload[2]),
			.id = mk_uint32_t(&rx_msg->payload[6]),
		};

		LOG_DBG("ModuleType: 0x%02x", info.type);
		LOG_DBG("devaddr: 0x%08"PRIx32, info.addr);
		LOG_DBG("dev id: 0x%08"PRIx32, info.id);
	}
}

//------------------------------------------------------------------------------
//
//  wimod_lorawan_DevMgmt_FirmwareVersion_Rsp
//
//  @brief: show firmware version
//
//------------------------------------------------------------------------------

static void wimod_lorawan_devmgmt_firmware_version_rsp(wimod_hci_message_t* rx_msg)
{
	char help[80];

	LOG_DBG("here");

	wimod_lorawan_show_response("firmware version response", wimod_device_mgmt_status_strings, rx_msg->payload[0]);

	if (rx_msg->payload[0] == DEVMGMT_STATUS_OK)
	{
		LOG_DBG("version: V%d.%d", (int)rx_msg->payload[2], (int)rx_msg->payload[1]);
		LOG_DBG("build-count: %d", (int)MAKEWORD(rx_msg->payload[3], rx_msg->payload[4]));

		memcpy(help, &rx_msg->payload[5], 10);
		help[10] = 0;
		LOG_DBG("build-date: %s", help);

		// more information attached ?
		if (rx_msg->length > 15)
		{
			// add string termination
			rx_msg->payload[rx_msg->length] = 0;
			LOG_DBG("firmware-content: %s", &rx_msg->payload[15]);
		}
	}
}

static void wimod_lorawan_get_opmode_rsp(wimod_hci_message_t* rx_msg)
{
	wimod_lorawan_show_response("get opmode response", wimod_device_mgmt_status_strings, rx_msg->payload[0]);
	LOG_DBG("Operation mode: %d", rx_msg->payload[1]);
}

static void wimod_lorawan_get_rtc_rsp(wimod_hci_message_t* rx_msg)
{
	u32_t rtc_value;

	wimod_lorawan_show_response("get rtc response", wimod_device_mgmt_status_strings, rx_msg->payload[0]);

	if (rx_msg->payload[0] == DEVMGMT_STATUS_OK)
	{
		rtc_value = MAKELONG(MAKEWORD(rx_msg->payload[4], rx_msg->payload[3]),
					MAKEWORD(rx_msg->payload[2], rx_msg->payload[1]));

		rtc_value = MAKELONG(MAKEWORD(rx_msg->payload[1], rx_msg->payload[2]),
					MAKEWORD(rx_msg->payload[3], rx_msg->payload[4]));

		LOG_DBG("RTC: %d-%02d-%02dT%02d:%02d:%02d", 2000+((rtc_value >> 26) & 0x3f)
							, (rtc_value >> 12) & 0x0f
							, (rtc_value >> 21) & 0x1f
							, (rtc_value >> 16) & 0x1f
							, (rtc_value >> 6) & 0x3f
							, (rtc_value >> 0) & 0x3f
		);

		LOG_DBG("RTC: 0x%08x", rtc_value);
	}
}

static void wimod_lorawan_get_rtc_alarm_rsp(wimod_hci_message_t* rx_msg)
{
	wimod_lorawan_show_response("get rtc alarm response", wimod_device_mgmt_status_strings, rx_msg->payload[0]);

	if (rx_msg->payload[0] == DEVMGMT_STATUS_OK)
	{
		LOG_DBG("Alarm Status: %d", rx_msg->payload[1]);
		LOG_DBG("Options: %d", rx_msg->payload[2]);
	}
}

static void wimod_lorawan_device_eui_rsp(wimod_hci_message_t* rx_msg)
{
	u32_t eui_lsb;
	u32_t eui_msb;
	wimod_lorawan_show_response("device EUI response",
			wimod_device_mgmt_status_strings, rx_msg->payload[0]);

	eui_lsb = MAKELONG(MAKEWORD(rx_msg->payload[8], rx_msg->payload[7]),
			MAKEWORD(rx_msg->payload[6], rx_msg->payload[5]));
	eui_msb = MAKELONG(MAKEWORD(rx_msg->payload[4], rx_msg->payload[3]),
			MAKEWORD(rx_msg->payload[2], rx_msg->payload[1]));

	LOG_INF("64-Bit Device EUI: 0x%08x%08x", eui_msb, eui_lsb);
}

static void wimod_lorawan_process_nwk_status_rsp(wimod_hci_message_t* rx_msg)
{
	uint32_t device_address;

	wimod_lorawan_show_response("network status response",
			wimod_device_mgmt_status_strings, rx_msg->payload[0]);

	LOG_DBG("Network Status: 0x%02X", rx_msg->payload[1]);

	if(rx_msg->length > 2){

		device_address = MAKELONG(MAKEWORD(rx_msg->payload[5], rx_msg->payload[4]),
					MAKEWORD(rx_msg->payload[3], rx_msg->payload[2]));

		LOG_DBG("Device Address: 0x%06"PRIx32, device_address);
		LOG_DBG("Data Rate Index: %d", rx_msg->payload[6]);
		LOG_DBG("Power Level: %d", rx_msg->payload[7]);
		LOG_DBG("Max Payload Size: %d", rx_msg->payload[8]);
	}
}

static void wimod_lorawan_process_rstack_config_rsp(wimod_hci_message_t* rx_msg)
{
	wimod_lorawan_show_response("radio stack config response",
			wimod_device_mgmt_status_strings, rx_msg->payload[0]);

	LOG_DBG("Default Data Rate Index: %d", rx_msg->payload[1]);
	LOG_DBG("Default TX Power Level: %d", rx_msg->payload[2]);
	LOG_DBG("Options: 0x%02X", rx_msg->payload[3]);
	LOG_DBG("\tAdaptive Data Rate: %s", (rx_msg->payload[3] & 0x01) ? "enabled" : "disabled");
	LOG_DBG("\tDuty Cycle Control: %s", (rx_msg->payload[3] & 0x02) ? "enabled" : "disabled");
	LOG_DBG("\tDevice Class: %s", (rx_msg->payload[3] & 0x04) ? "C" : "A");
	LOG_DBG("\tRF packet output format: %s", (rx_msg->payload[3] & 0x40) ? "extended" : "standard");
	LOG_DBG("\tRx MAC Command Forwarding: %s", (rx_msg->payload[3] & 0x80) ? "enabled" : "disabled");
	LOG_DBG("Power Saving Mode: %s", rx_msg->payload[4] ? "automatic" : "off");
	LOG_DBG("Number of Retransmissions: %d", rx_msg->payload[5]);
	LOG_DBG("Band Index: %d", rx_msg->payload[6]);
	// not available in 1.11 specs
	LOG_DBG("Header MAC Cmd Capacity: %d", rx_msg->payload[7] & 0xFF);

}

static void wimod_lorawan_process_send_data_rsp(wimod_hci_message_t* rx_msg)
{
	u32_t time_remaining_ms;

	wimod_lorawan_show_response("send C-Data response",
			wimod_device_mgmt_status_strings, rx_msg->payload[0]);

	if(rx_msg->length > 1){

		time_remaining_ms = MAKELONG(MAKEWORD(rx_msg->payload[1], rx_msg->payload[2]),
					MAKEWORD(rx_msg->payload[3], rx_msg->payload[4]));

		LOG_DBG("Channel blocked. Time remaining (ms): %d", time_remaining_ms);
	}
}

//------------------------------------------------------------------------------
//
//  Process_LoRaWAN_Message
//
//  @brief: handle received LoRaWAN SAP messages
//
//------------------------------------------------------------------------------

static void wimod_lorawan_process_lorawan_message(wimod_hci_message_t* rx_msg)
{
	LOG_DBG("msg_id:%d", rx_msg->msg_id);

	switch(rx_msg->msg_id)
	{
		case	LORAWAN_MSG_GET_DEVICE_EUI_RSP:
				wimod_lorawan_device_eui_rsp(rx_msg);
				break;

		case	LORAWAN_MSG_JOIN_NETWORK_RSP:
				wimod_lorawan_show_response("join network response", wimod_lorawan_status_strings, rx_msg->payload[0]);
				break;

		case	LORAWAN_MSG_FACTORY_RESET_RSP:
				wimod_lorawan_show_response("factory reset response", wimod_lorawan_status_strings, rx_msg->payload[0]);
				break;

		case	LORAWAN_MSG_SET_JOIN_PARAM_RSP:
				wimod_lorawan_show_response("set join param response", wimod_lorawan_status_strings, rx_msg->payload[0]);
				break;

		case	LORAWAN_MSG_SET_RSTACK_CONFIG_RSP:
				wimod_lorawan_show_response("set rstack response", wimod_lorawan_status_strings, rx_msg->payload[0]);
				break;

		case	LORAWAN_MSG_SEND_UDATA_RSP:
				wimod_lorawan_show_response("send U-Data response", wimod_lorawan_status_strings, rx_msg->payload[0]);
				break;

		case	LORAWAN_MSG_SEND_CDATA_RSP:
				wimod_lorawan_process_send_data_rsp(rx_msg);
				//wimod_lorawan_show_response("send C-Data response", wimod_lorawan_status_strings, rx_msg->payload[0]);
				break;

		case	LORAWAN_MSG_GET_NWK_STATUS_RSP:
				wimod_lorawan_process_nwk_status_rsp(rx_msg);
				break;

		case	LORAWAN_MSG_GET_RSTACK_CONFIG_RSP:
				wimod_lorawan_process_rstack_config_rsp(rx_msg);
				break;

		case	LORAWAN_MSG_JOIN_NETWORK_TX_IND:
				wimod_lorawan_process_join_tx_indication(rx_msg);
				break;

		case	LORAWAN_MSG_SEND_UDATA_TX_IND:
				wimod_lorawan_show_response_tx("send U-Data TX indication", wimod_lorawan_status_strings, rx_msg->payload[0]);
				break;

		case	LORAWAN_MSG_SEND_CDATA_TX_IND:
				wimod_lorawan_show_response_tx("send C-Data TX indication", wimod_lorawan_status_strings, rx_msg->payload[0]);
				break;

		case	LORAWAN_MSG_JOIN_NETWORK_IND:
				wimod_lorawan_process_join_network_indication(rx_msg);
				break;

		case	LORAWAN_MSG_RECV_UDATA_IND:
				wimod_lorawan_process_u_data_rx_indication(rx_msg);
				break;

		case	LORAWAN_MSG_RECV_CDATA_IND:
				wimod_lorawan_process_c_data_rx_indication(rx_msg);
				break;

		case	LORAWAN_MSG_RECV_NO_DATA_IND:
				LOG_DBG("no data received indication");
				if(rx_msg->payload[0] == 1){
					LOG_DBG("Error Code: 0x%02X", rx_msg->payload[1]);
				}
				break;

		default:
				LOG_DBG("unhandled LoRaWAN SAP message received - msg_id : 0x%02X", (u8_t)rx_msg->msg_id);
				break;
	}
}

//------------------------------------------------------------------------------
//
//  wimod_lorawan_process_JoinTxIndication
//
//  @brief: show join transmit indicaton
//
//------------------------------------------------------------------------------

static void wimod_lorawan_process_join_tx_indication(wimod_hci_message_t* rx_msg)
{
	if (rx_msg->payload[0] == 0)
	{
		LOG_DBG("join tx event - Status : ok");
	}
	// channel info attached ?
	else if(rx_msg->payload[0] == 1)
	{
		LOG_DBG("join tx event:%d, ChnIdx:%d, DR:%d - Status : ok", (int)rx_msg->payload[3], (int)rx_msg->payload[1], (int)rx_msg->payload[2]);
	}
	else
	{
		LOG_DBG("join tx event - Status : error");
	}
}

//------------------------------------------------------------------------------
//
//  wimod_lorawan_process_JoinNetworkIndication
//
//  @brief: show join network indicaton
//
//------------------------------------------------------------------------------

void wimod_lorawan_process_join_network_indication(wimod_hci_message_t* rx_msg)
{
	if (rx_msg->payload[0] == 0)
	{
		u32_t address = MAKELONG(MAKEWORD(rx_msg->payload[1],rx_msg->payload[2]),
								MAKEWORD(rx_msg->payload[3],rx_msg->payload[4]));

		LOG_DBG("join network accept event - DeviceAddress:0x%08X", address);

		if(join_callback){
			(*join_callback)();
		}
	}
	else if (rx_msg->payload[0] == 1)
	{
		u32_t address = MAKELONG(MAKEWORD(rx_msg->payload[1],rx_msg->payload[2]),
								MAKEWORD(rx_msg->payload[3],rx_msg->payload[4]));

		LOG_DBG("join network accept event - DeviceAddress:0x%08X, ChnIdx:%d, DR:%d, RSSI:%d, SNR:%d, RxSlot:%d", address,
			(int)rx_msg->payload[5], (int)rx_msg->payload[6], (int)rx_msg->payload[7],
			(int)rx_msg->payload[8], (int)rx_msg->payload[9]);

		if(join_callback){
			(*join_callback)();
		}
	}
	else
	{
		LOG_DBG("join network timeout event");
	}
}

//------------------------------------------------------------------------------
//
//  wimod_lorawan_process_U_DataRxIndication
//
//  @brief: show received U-Data
//
//------------------------------------------------------------------------------

void wimod_lorawan_process_u_data_rx_indication(wimod_hci_message_t* rx_msg)
{
	int payload_size = rx_msg->length - 1;

	// rx channel info attached ?
	if (rx_msg->payload[0] & 0x01)
		payload_size -= 5;

	if (payload_size >= 1)
	{
		LOG_DBG("U-Data rx event: port:0x%02Xapp-payload:", rx_msg->payload[1]);
		for(int i = 1; i < payload_size; i++)
			LOG_DBG("%02X ", rx_msg->payload[1+i]);
		LOG_DBG("");
		wimod_data_rx_handler(rx_msg->payload[1], &rx_msg->payload[2], payload_size-1);
	}

	if (rx_msg->payload[0] & 0x02)
		LOG_DBG("ack for uplink packet:yes");
	else
		LOG_DBG("ack for uplink packet:no");

	if (rx_msg->payload[0] & 0x04)
		LOG_DBG("frame pending:yes");
	else
		LOG_DBG("frame pending:no");

	// rx channel info attached ?
	if (rx_msg->payload[0] & 0x01)
	{
		u8_t* rxInfo = &rx_msg->payload[1 + payload_size];
		LOG_DBG("ChnIdx:%d, DR:%d, RSSI:%d, SNR:%d, RxSlot:%d",
			(int)rxInfo[0], (int)rxInfo[1], (int)rxInfo[2],
			(int)rxInfo[3], (int)rxInfo[4]);
	}
}

//------------------------------------------------------------------------------
//
//  wimod_lorawan_process_C_DataRxIndication
//
//  @brief: show received C-Data
//
//------------------------------------------------------------------------------

void wimod_lorawan_process_c_data_rx_indication(wimod_hci_message_t* rx_msg)
{
	int payload_size = rx_msg->length - 1;

	// rx channel info attached ?
	if (rx_msg->payload[0] & 0x01)
		payload_size -= 5;

	if (payload_size >= 1)
	{
		LOG_DBG("C-Data rx event: port:0x%02Xapp-payload:", rx_msg->payload[1]);
		for(int i = 1; i < payload_size;)
			LOG_DBG("%02X ", rx_msg->payload[1+i]);
		LOG_DBG("");

		wimod_data_rx_handler(rx_msg->payload[1], &rx_msg->payload[2], payload_size);
	}

	if (rx_msg->payload[0] & 0x02)
		LOG_DBG("ack for uplink packet:yes");
	else
		LOG_DBG("ack for uplink packet:no");

	if (rx_msg->payload[0] & 0x04)
		LOG_DBG("frame pending:yes");
	else
		LOG_DBG("frame pending:no");

	// rx channel info attached ?
	if (rx_msg->payload[0] & 0x01)
	{
		u8_t* rxInfo = &rx_msg->payload[1 + payload_size];
		LOG_DBG("ChnIdx:%d, DR:%d, RSSI:%d, SNR:%d, RxSlot:%d",
			(int)rxInfo[0], (int)rxInfo[1], (int)rxInfo[2],
			(int)rxInfo[3], (int)rxInfo[4]);
	}
}

//------------------------------------------------------------------------------
//
//  wimod_lorawan_show_response
//
//  @brief: show response status as human readable string
//
//------------------------------------------------------------------------------

static void wimod_lorawan_show_response(const char* str, const id_string_t* status_table, u8_t status_id)
{
	while(status_table->string)
	{
		if (status_table->id == status_id)
		{
			LOG_DBG("%s: - Status(0x%02X) : %s", str, status_id, status_table->string);
			return;
		}

		status_table++;
	}
}

static void wimod_lorawan_show_response_tx(const char* str, const id_string_t* status_table, u8_t status_id)
{
	/* status for IND_TX messages are OK when status is 0x01.
	* So display status 0x01, and string for 0x00.
	*/
	uint32_t status_id2 = (status_id == 0x01 ? DEVMGMT_STATUS_OK : status_id);

	while(status_table->string)
	{
		if (status_table->id == status_id2)
		{
			LOG_DBG("%s: - Status(0x%02X) : %s", str, status_id, status_table->string);
			return;
		}

		status_table++;
	}
}
//------------------------------------------------------------------------------
// end of file
//------------------------------------------------------------------------------
