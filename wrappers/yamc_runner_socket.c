/*
 * YAMC - Yet Another MQTT Client library
 *
 * yamc_runner_socket.c - Quick and dirty Unix TCP socket wrapper. Allows YAMC testing on PC.
 *
 * Author: Michal Lower <https://github.com/keton>
 * 
 * Licensed under MIT License (see LICENSE file in main repo directory)
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
	yamc_net_core.instance.parser_enables.PUBCOMP  = true;
	yamc_net_core.instance.parser_enables.PUBREL   = true;
	yamc_net_core.instance.parser_enables.PUBREC  = true;
	yamc_net_core.instance.parser_enables.UNSUBACK = true;

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

	// subscribe to topics
	yamc_subscribe_data_t subscribe_data[2];
	memset(subscribe_data, 0, sizeof(subscribe_data));
	yamc_char_to_mqtt_str("test1/#", &subscribe_data[0].topic);
	yamc_char_to_mqtt_str("test2/#", &subscribe_data[1].topic);

	ret = yamc_subscribe(&yamc_net_core.instance, subscribe_data, sizeof(subscribe_data) / sizeof(subscribe_data[0]));
	if (ret != YAMC_RET_SUCCESS)
	{
		printf("Error sending subscribe packet: %u\n", ret);
		exit(-1);
	}

	// unsubscribe 'test2/#'
	yamc_mqtt_string unsubscribe_data[1];
	memset(unsubscribe_data, 0, sizeof(unsubscribe_data));
	yamc_char_to_mqtt_str("test2/#", &unsubscribe_data[0]);

	ret = yamc_unsubscribe(&yamc_net_core.instance, unsubscribe_data, sizeof(unsubscribe_data) / sizeof(unsubscribe_data[0]));
	if (ret != YAMC_RET_SUCCESS)
	{
		printf("Error sending subscribe packet: %u\n", ret);
		exit(-1);
	}

	// send MQTT publish packet
	yamc_publish_data_t publish_data;
	memset(&publish_data, 0, sizeof(yamc_publish_data_t));

	publish_data.QOS = YAMC_QOS_LVL1;
	yamc_char_to_mqtt_str("test/hello", &publish_data.topic);
	yamc_publish_set_char_payload("Hello world!", &publish_data);

	ret = yamc_publish(&yamc_net_core.instance, &publish_data);
	if (ret != YAMC_RET_SUCCESS)
	{
		printf("Error sending publish packet: %u\n", ret);
		exit(-1);
	}

	// repeatedly send ping request to keep connection alive
	while (!yamc_net_core_should_exit(&yamc_net_core))
	{
		ret = yamc_ping(&yamc_net_core.instance);
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
