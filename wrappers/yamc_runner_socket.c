/*
 * YAMC - Yet Another MQTT Client library
 *
 * yamc_runner_socket.c - Quick and dirty Unix TCP socket wrapper. Allows YAMC testing on PC.
 *
 * Author: Michal Lower <keton22@gmail.com>
 *
 * All rights reserved 2017
 *
 */

#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "yamc.h"
#include "yamc_port.h"

#include "yamc_debug_pkt_handler.h"  //example user defined packet handlers, dump everything to console
#include "yamc_net_core.h"

yamc_net_core_t yamc_net_core;

int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		YAMC_ERROR_PRINTF("usage %s hostname port\n", argv[0]);
		exit(0);
	}

	char* hostname = argv[1];
	int   portno   = atoi(argv[2]);

	memset(&yamc_net_core, 0, sizeof(yamc_net_core));

	yamc_net_core_connect(&yamc_net_core, hostname, portno, yamc_debug_pkt_handler_main);

	// Create receive data thread
	YAMC_DEBUG_PRINTF("Connected launching rx thread...\n");

	// show startup messages on console
	fflush(stdout);

	// enable pkt_handler for following packet types
	yamc_net_core.instance.parser_enables.CONNACK  = true;
	yamc_net_core.instance.parser_enables.PUBLISH  = true;
	yamc_net_core.instance.parser_enables.PUBACK   = true;
	yamc_net_core.instance.parser_enables.PINGRESP = true;
	yamc_net_core.instance.parser_enables.SUBACK   = true;

	// process sending data to socket here...

	// send MQTT connect packet
	yamc_retcode_t		ret;
	yamc_connect_data_t connect_data;

	memset(&connect_data, 0, sizeof(yamc_connect_data_t));

	connect_data.clean_session		 = true;
	connect_data.keepalive_timeout_s = 30;

	ret = yamc_connect(&yamc_net_core.instance, &connect_data);
	if (ret != YAMC_RET_SUCCESS)
	{
		printf("Error sending connect packet: %u\n", ret);
		exit(-1);
	}

	// send MQTT publish packet
	yamc_publish_data_t publish_data;
	memset(&publish_data, 0, sizeof(yamc_publish_data_t));

	publish_data.QOS=YAMC_QOS_LVL1;
	yamc_char_to_mqtt_str("test/hello", &publish_data.topic);
	yamc_publish_set_char_payload("Hello world!", &publish_data);

	ret = yamc_publish(&yamc_net_core.instance, &publish_data);
	if (ret != YAMC_RET_SUCCESS)
	{
		printf("Error sending publish packet: %u\n", ret);
		exit(-1);
	}

	// subscribe to topic 'test/#'
	yamc_mqtt_pkt_data_t subscribe_pkt;
	memset(&subscribe_pkt, 0, sizeof(yamc_mqtt_pkt_data_t));

	yamc_mqtt_pkt_subscribe_topic_t subscribe_topics[2];
	memset(subscribe_topics, 0, sizeof(subscribe_topics));

	char topic1[] = "test/#";
	char topic2[] = "yamc_test/#";

	subscribe_topics[0].qos.lvl		   = YAMC_QOS_LVL0;
	subscribe_topics[0].topic_name.str = (uint8_t*)topic1;
	subscribe_topics[0].topic_name.len = strlen(topic1);

	subscribe_topics[1].qos.lvl		   = YAMC_QOS_LVL0;
	subscribe_topics[1].topic_name.str = (uint8_t*)topic2;
	subscribe_topics[1].topic_name.len = strlen(topic2);

	subscribe_pkt.pkt_type								= YAMC_PKT_SUBSCRIBE;
	subscribe_pkt.pkt_data.subscribe.pkt_id				= 1338;
	subscribe_pkt.pkt_data.subscribe.payload.p_topics   = subscribe_topics;
	subscribe_pkt.pkt_data.subscribe.payload.topics_len = sizeof(subscribe_topics) / sizeof(subscribe_topics[0]);

	ret = yamc_send_pkt(&yamc_net_core.instance, &subscribe_pkt);
	if (ret != YAMC_RET_SUCCESS)
	{
		printf("Error sending subscribe packet: %u\n", ret);
		exit(-1);
	}

	//unsubscribe topic 'test/#'
	/*
	yamc_mqtt_pkt_data_t unsubscribe_pkt;
	memset(&unsubscribe_pkt, 0, sizeof(yamc_mqtt_pkt_data_t));

	yamc_mqtt_string unsubscribe_topics[2];
	memset(subscribe_topics, 0, sizeof(subscribe_topics));

	char unsub_topic1[] = "test/#";
	char unsub_topic2[] = "yamc_test/#";

	unsubscribe_topics[0].str = (uint8_t*)unsub_topic1;
	unsubscribe_topics[0].len = strlen(unsub_topic1);

	unsubscribe_topics[1].str = (uint8_t*)unsub_topic2;
	unsubscribe_topics[1].len = strlen(unsub_topic2);

	unsubscribe_pkt.pkt_type								= YAMC_PKT_UNSUBSCRIBE;
	unsubscribe_pkt.pkt_data.unsubscribe.pkt_id				= 1339;
	unsubscribe_pkt.pkt_data.unsubscribe.payload.p_topics   = unsubscribe_topics;
	unsubscribe_pkt.pkt_data.unsubscribe.payload.topics_len = sizeof(unsubscribe_topics) / sizeof(unsubscribe_topics[0]);

	ret = yamc_send_pkt(&yamc_net_core.instance, &unsubscribe_pkt);
	if (ret != YAMC_RET_SUCCESS)
	{
		printf("Error sending unsubscribe packet: %u\n", ret);
		exit(-1);
	}
	*/

	// repeatedly send ping request to keep connection alive
	while (!yamc_net_core_should_exit(&yamc_net_core))
	{
		yamc_mqtt_pkt_data_t pingreq_pkt;
		memset(&pingreq_pkt, 0, sizeof(yamc_mqtt_pkt_data_t));

		pingreq_pkt.pkt_type = YAMC_PKT_PINGREQ;

		ret = yamc_send_pkt(&yamc_net_core.instance, &pingreq_pkt);
		if (ret != YAMC_RET_SUCCESS)
		{
			printf("Error sending pingreq packet: %u\n", ret);
			exit(-1);
		}

		sleep(25);
	}

	// cleanup
	yamc_net_core_disconnect(&yamc_net_core);

	return 0;
}
