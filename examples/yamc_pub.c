/*
 * YAMC - Yet Another MQTT Client library
 *
 * yamc_pub.c - Simple MQTT client example. Publishes message to MQTT server and quits.
 *
 * Author: Michal Lower <https://github.com/keton>
 * 
 * Licensed under MIT License (see LICENSE file in main repo directory)
 * 
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include "yamc.h"
#include "yamc_net_core.h"
#include "yamc_pub_cmdline.h"

static volatile bool connack_received = false;
static volatile bool publish_complete = false;

static inline void yamc_handle_connack(yamc_instance_t* const p_instance, const yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_UNUSED_PARAMETER(p_instance);

	if (p_pkt_data->pkt_data.connack.return_code != YAMC_CONNACK_ACCEPTED)
	{
		YAMC_ERROR_PRINTF("Server rejected connection with code: %u\n", p_pkt_data->pkt_data.connack.return_code);
		exit(-1);
	}

	connack_received = true;
}

static inline void yamc_handle_pub_x(yamc_instance_t* const p_instance, const yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	yamc_retcode_t ret = YAMC_RET_SUCCESS;

	switch (p_pkt_data->pkt_type)
	{
		case YAMC_PKT_PUBREC:
			ret = yamc_pubrel(p_instance, p_pkt_data->pkt_data.pubrec.packet_id);
			break;

		case YAMC_PKT_PUBREL:
			ret = yamc_pubcomp(p_instance, p_pkt_data->pkt_data.pubrel.packet_id);
			break;

		default:
			break;
	}

	if (ret != YAMC_RET_SUCCESS)
	{
		YAMC_ERROR_PRINTF("Error sending QoS message: %u\n", ret);
		exit(-1);
	}
}

static void yamc_pub_pkt_handler(yamc_instance_t* const p_instance, const yamc_mqtt_pkt_data_t* const p_pkt_data, void* p_ctx)
{
	YAMC_UNUSED_PARAMETER(p_ctx);

	// decode data according to packet type
	switch (p_instance->rx_pkt.fixed_hdr.pkt_type.flags.type)
	{
		case YAMC_PKT_CONNACK:
			yamc_handle_connack(p_instance, p_pkt_data);
			break;

		case YAMC_PKT_PUBREC:
		case YAMC_PKT_PUBREL:
			yamc_handle_pub_x(p_instance, p_pkt_data);
			break;
		case YAMC_PKT_PUBACK:
		case YAMC_PKT_PUBCOMP:
			publish_complete = true;
			break;

		default:
			break;
	}
}

int main(int argc, char** argv)
{
	struct yamc_pub_args_info args_info;
	if (yamc_pub_cmd_parser(argc, argv, &args_info) != 0)
	{
		yamc_pub_cmd_parser_print_help();
		exit(1);
	}

	yamc_net_core_t yamc_net_core;
	memset(&yamc_net_core, 0, sizeof(yamc_net_core));
	yamc_net_core_connect(&yamc_net_core, args_info.host_arg, args_info.port_arg, yamc_pub_pkt_handler);

	// enable pkt_handler for following packet types
	yamc_net_core.instance.parser_enables.CONNACK = true;
	yamc_net_core.instance.parser_enables.PUBACK  = true;
	yamc_net_core.instance.parser_enables.PUBCOMP = true;
	yamc_net_core.instance.parser_enables.PUBREL  = true;
	yamc_net_core.instance.parser_enables.PUBREC  = true;

	// send MQTT connect packet
	yamc_retcode_t		ret;
	yamc_connect_data_t connect_data;

	memset(&connect_data, 0, sizeof(yamc_connect_data_t));

	connect_data.clean_session		 = !args_info.no_clean_session_flag;
	connect_data.keepalive_timeout_s = args_info.keepalive_timeout_arg;

	if (args_info.client_id_arg)
	{
		yamc_char_to_mqtt_str(args_info.client_id_arg, &connect_data.client_id);
	}

	if (args_info.user_arg)
	{
		yamc_char_to_mqtt_str(args_info.user_arg, &connect_data.user_name);
	}

	if (args_info.password_arg)
	{
		yamc_char_to_mqtt_str(args_info.password_arg, &connect_data.password);
	}

	connect_data.will_remain = args_info.will_remain_flag;
	connect_data.will_qos	= args_info.will_qos_arg;

	if (args_info.will_topic_arg)
	{
		yamc_char_to_mqtt_str(args_info.will_topic_arg, &connect_data.will_topic);
	}

	if (args_info.will_msg_arg)
	{
		yamc_char_to_mqtt_str(args_info.will_msg_arg, &connect_data.will_message);
	}

	ret = yamc_connect(&yamc_net_core.instance, &connect_data);
	if (ret != YAMC_RET_SUCCESS)
	{
		YAMC_ERROR_PRINTF("Error sending connect packet: %u\n", ret);
		exit(-1);
	}

	//wait for connack packet to arrive
	while (!connack_received)
	{
		usleep(5000);
	}

	// send MQTT publish packet
	yamc_publish_data_t publish_data;
	memset(&publish_data, 0, sizeof(yamc_publish_data_t));

	publish_data.QOS = args_info.qos_arg;
	yamc_char_to_mqtt_str(args_info.topic_arg, &publish_data.topic);

	//payload can be empty
	if (args_info.message_arg) yamc_publish_set_char_payload(args_info.message_arg, &publish_data);

	ret = yamc_publish(&yamc_net_core.instance, &publish_data);
	if (ret != YAMC_RET_SUCCESS)
	{
		YAMC_ERROR_PRINTF("Error sending publish packet: %u\n", ret);
		exit(-1);
	}

	//for qos 0 there will be no confirmation
	if (args_info.qos_arg == 0) publish_complete = true;

	//wait for publish confirmation packet to arrive
	while (!publish_complete)
	{
		usleep(5000);
	}

	// cleanup
	yamc_net_core_disconnect(&yamc_net_core);

	return 0;
}
