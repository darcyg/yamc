/*
 * YAMC - Yet Another MQTT Client library
 *
 * yamc_sub.c - Simple MQTT client example. Subscribes to topic and waits for the messages.
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
#include "yamc_sub_cmdline.h"

static volatile bool connack_received = false;
static volatile bool suback_received  = false;

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

static inline void yamc_handle_publish(const yamc_instance_t* const p_instance, const yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_ASSERT(p_instance != NULL);
	YAMC_ASSERT(p_pkt_data != NULL);

	const yamc_mqtt_pkt_publish_t* const p_data = &p_pkt_data->pkt_data.publish;

	YAMC_DEBUG_PRINTF("\"%.*s\": \"%.*s\"\n", p_data->topic_name.len, p_data->topic_name.str, p_data->payload.data_len,
					  p_data->payload.p_data);

	yamc_retcode_t ret=YAMC_RET_SUCCESS;

	//send puback for QoS1
	if (p_pkt_data->flags.QOS == YAMC_QOS_LVL1)
	{
		ret = yamc_puback(p_instance, p_data->packet_id);
	}

	//send pubrec for QoS2
	if (p_pkt_data->flags.QOS == YAMC_QOS_LVL2)
	{
		ret = yamc_pubrec(p_instance, p_data->packet_id);
	}

	if (ret != YAMC_RET_SUCCESS)
	{
		YAMC_ERROR_PRINTF("Error sending QoS packet: %u\n", ret);
		exit(-1);
	}
}

static inline void yamc_handle_suback(const yamc_instance_t* const p_instance, const yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_ASSERT(p_instance != NULL);
	YAMC_ASSERT(p_pkt_data != NULL);

	const yamc_mqtt_pkt_suback_t* const p_data = &p_pkt_data->pkt_data.suback;

	for (uint16_t i = 0; i < p_data->payload.retcodes_len; i++)
	{
		if (p_data->payload.p_retcodes[i] == YAMC_SUBACK_FAIL)
		{
			YAMC_ERROR_PRINTF("Server rejected subscription for topic index: %u\n", i);
			exit(-1);
		}
	}

	suback_received = true;
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

		case YAMC_PKT_SUBACK:
			yamc_handle_suback(p_instance, p_pkt_data);
			break;

		case YAMC_PKT_PUBLISH:
			yamc_handle_publish(p_instance, p_pkt_data);
			break;

		default:
			break;
	}
}

int main(int argc, char** argv)
{
	struct yamc_sub_args_info args_info;
	if (yamc_sub_cmd_parser(argc, argv, &args_info) != 0)
	{
		yamc_sub_cmd_parser_print_help();
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
	yamc_net_core.instance.parser_enables.PUBLISH = true;
	yamc_net_core.instance.parser_enables.SUBACK  = true;

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

	size_t						 subscribe_buff_len = args_info.topic_given * sizeof(yamc_subscribe_data_t);
	yamc_subscribe_data_t* const subscribe_data		= malloc(subscribe_buff_len);

	if (!subscribe_data)
	{
		YAMC_ERROR_PRINTF("Failed to allocate %zu bytes!\n", subscribe_buff_len);
		exit(-1);
	}

	for (unsigned int i = 0; i < args_info.topic_given; i++)
	{
		yamc_char_to_mqtt_str(args_info.topic_arg[i], &subscribe_data[i].topic);
		subscribe_data[i].qos = args_info.qos_arg;
	}

	ret = yamc_subscribe(&yamc_net_core.instance, subscribe_data, args_info.topic_given);
	if (ret != YAMC_RET_SUCCESS)
	{
		YAMC_ERROR_PRINTF("Error sending subscribe packet: %u\n", ret);
		exit(-1);
	}
	free(subscribe_data);

	//wait for suback packet to arrive
	while (!suback_received)
	{
		usleep(5000);
	}

	// repeatedly send ping request to keep connection alive
	while (!yamc_net_core_should_exit(&yamc_net_core))
	{
		ret = yamc_ping(&yamc_net_core.instance);
		if (ret != YAMC_RET_SUCCESS)
		{
			YAMC_ERROR_PRINTF("Error sending pingreq packet: %u\n", ret);
			exit(-1);
		}

		sleep(args_info.keepalive_timeout_arg / 2);
	}

	// cleanup
	yamc_net_core_disconnect(&yamc_net_core);

	return 0;
}
