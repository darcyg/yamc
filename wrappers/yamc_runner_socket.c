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

#include <yamc_debug_pkt_handler.h>  //example user defined packet handlers, dump everything to console

// if true server has disconnected, exit
volatile uint8_t exit_now = false;

// server socket handle
static int server_socket = -1;

// read_sock_thr thread handle
static pthread_t rx_tid;

static yamc_instance_t yamc_instance;

// timeout timer settings
#define YAMC_TIMEOUT_S 30  // seconds
#define YAMC_TIMEOUT_NS 0  // nanoseconds

// timeout timer handle
static timer_t timeout_timer;

// timeout timer signal handler
static void timeout_handler(sigval_t sigval)
{
	YAMC_UNUSED_PARAMETER(sigval);

	YAMC_ERROR_PRINTF("Timeout!\n");
	fflush(stderr);

	exit(-1);
}

// start/prolong timeout timer wrapper
static void timeout_pat(void)
{
	struct itimerspec its;
	memset(&its, 0, sizeof(struct itimerspec));
	its.it_value.tv_sec  = YAMC_TIMEOUT_S;
	its.it_value.tv_nsec = YAMC_TIMEOUT_NS;

	int err_code = timer_settime(timeout_timer, 0, &its, NULL);
	if (err_code < 0)
	{
		YAMC_ERROR_PRINTF("Timer error\n");
		exit(-1);
	}
}

// stop timeout timer
static void timeout_stop(void)
{
	struct itimerspec its;
	memset(&its, 0, sizeof(struct itimerspec));

	int err_code = timer_settime(timeout_timer, 0, &its, NULL);
	if (err_code < 0)
	{
		YAMC_ERROR_PRINTF("Timer error\n");
		exit(-1);
	}
}

// timeout timer setup
static void setup_timer(void)
{
	int err_code = -1;

	struct sigevent sev;

	memset(&sev, 0, sizeof(struct sigevent));

	sev.sigev_signo			  = SIGRTMIN;		  // signal type
	sev.sigev_notify		  = SIGEV_THREAD;	 // call timeout handler as if it was starting a new thread
	sev.sigev_notify_function = timeout_handler;  // set timeout handler

	if ((err_code = timer_create(CLOCK_REALTIME, &sev, &timeout_timer)) < 0)
	{
		YAMC_ERROR_PRINTF("ERROR setting up timeout timer: %s\n", strerror(err_code));
		exit(0);
	}
}

// write to socket wrapper
static yamc_retcode_t socket_write_buff(uint8_t* buff, uint32_t len)
{
	ssize_t n = write(server_socket, buff, len);
	if (n < 0)
	{
		YAMC_ERROR_PRINTF("ERROR writing to socket:%s\n", strerror(n));
		return YAMC_RET_INVALID_STATE;
	}
	if (n != len)
	{
		YAMC_ERROR_PRINTF("Incomplete socket write!");
	}
	return YAMC_RET_SUCCESS;
}

static void disconnect_handler(void)
{
	YAMC_ERROR_PRINTF("yamc requested to drop connection!\n");
	exit(-1);
}

// receive data from socket thread
static void* read_sock_thr(void* p_ctx)
{
	YAMC_UNUSED_PARAMETER(p_ctx);

	// buffer for incoming data
	uint8_t rx_buff[10];

	// how many bytes were received in single read operation or read() error code
	int rx_bytes = 0;

	do
	{
		// zero rx buffer
		memset(rx_buff, 0, sizeof(rx_buff));

		rx_bytes = read(server_socket, rx_buff, sizeof(rx_buff));

		// there was error code thrown by read()
		if (rx_bytes < 0)
		{
			YAMC_ERROR_PRINTF("TCP read() error: %s\n", strerror(rx_bytes));
			exit_now = true;
			pthread_exit(&rx_bytes);
		}

		// process buffer here
		if (rx_bytes > 0) yamc_parse_buff(&yamc_instance, rx_buff, rx_bytes);

	} while (rx_bytes > 0);

	exit_now = true;
	return NULL;
}

// connect socket to specified host and port
static void setup_socket(int* p_socket, char* hostname, int portno)
{
	// server socket address struct
	struct sockaddr_in serv_addr;

	// host data struct pointer
	struct hostent* server;

	int err_code = 0;

	memset(&serv_addr, 0, sizeof(serv_addr));

	// create socket
	*p_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (*p_socket < 0) YAMC_ERROR_PRINTF("ERROR opening socket\n");

	// perform host name lookup
	server = gethostbyname(hostname);
	if (server == NULL)
	{
		YAMC_ERROR_PRINTF("ERROR, no such host\n");
		exit(0);
	}

	// prepare data for connect()

	// this is Internet (TCP) connection
	serv_addr.sin_family = AF_INET;

	// copy connection IP address
	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

	// set connection port, convert number format to network byte order
	serv_addr.sin_port = htons(portno);

	// connect
	if ((err_code = connect(*p_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0)
	{
		YAMC_ERROR_PRINTF("ERROR connecting: %s\n", strerror(err_code));
		exit(0);
	}
}

//handle shutdown (i.e. Ctrl+C) in graceful fashion
static void sigint_handler(int signal)
{
	YAMC_UNUSED_PARAMETER(signal);

	// send MQTT disconnect packet
	yamc_mqtt_pkt_data_t disconnect_pkt;
	memset(&disconnect_pkt, 0, sizeof(yamc_mqtt_pkt_data_t));

	disconnect_pkt.pkt_type = YAMC_PKT_DISCONNECT;

	yamc_retcode_t ret = yamc_send_pkt(&yamc_instance, &disconnect_pkt);
	if (ret != YAMC_RET_SUCCESS)
	{
		printf("Error sending disconnect packet: %u\n", ret);
		exit(-1);
	}

	//signal that we should go now...
	exit_now = true;
}

//setup signal handler for Crtl+C
static void setup_sigint_handler(void)
{
	struct sigaction sigint_action;
	memset(&sigint_action, 0, sizeof(struct sigaction));
	sigint_action.sa_handler = sigint_handler;
	sigemptyset(&sigint_action.sa_mask);
	sigint_action.sa_flags = 0;
	sigaction(SIGINT, &sigint_action, NULL);
	sigaction(SIGHUP, &sigint_action, NULL);
	sigaction(SIGTERM, &sigint_action, NULL);
}

int main(int argc, char* argv[])
{
	int portno;

	if (argc < 3)
	{
		YAMC_ERROR_PRINTF("usage %s hostname port\n", argv[0]);
		exit(0);
	}

	// setup timeout timer
	setup_timer();

	portno = atoi(argv[2]);

	// setup socket and connect to server
	setup_socket(&server_socket, argv[1], portno);

	//setup disconnect on signal
	setup_sigint_handler();

	// Create receive data thread
	YAMC_DEBUG_PRINTF("Connected launching rx thread...\n");

	// show startup messages on console
	fflush(stdout);

	yamc_handler_cfg_t handler_cfg = {.disconnect   = disconnect_handler,
									  .write		= socket_write_buff,
									  .timeout_pat  = timeout_pat,
									  .timeout_stop = timeout_stop,
									  .pkt_handler  = yamc_debug_pkt_handler_main};

	yamc_init(&yamc_instance, &handler_cfg);

	// enable pkt_handler for following packet types
	yamc_instance.parser_enables.CONNACK  = true;
	yamc_instance.parser_enables.PUBLISH  = true;
	yamc_instance.parser_enables.PUBACK   = true;
	yamc_instance.parser_enables.PINGRESP = true;
	yamc_instance.parser_enables.SUBACK   = true;

	// create thread
	pthread_create(&rx_tid, NULL, read_sock_thr, NULL);

	// process sending data to socket here...

	// send MQTT connect packet
	yamc_mqtt_pkt_data_t connect_pkt;
	memset(&connect_pkt, 0, sizeof(yamc_mqtt_pkt_data_t));

	// basic configuration
	char client_id[] = "test_clinet_id";

	connect_pkt.pkt_type										   = YAMC_PKT_CONNECT;
	connect_pkt.pkt_data.connect.client_id.str					   = (uint8_t*)client_id;
	connect_pkt.pkt_data.connect.client_id.len					   = strlen(client_id);
	connect_pkt.pkt_data.connect.connect_flags.flags.clean_session = 1;
	connect_pkt.pkt_data.connect.keepalive_timeout_s			   = 60;

	/*
	//client authentication
	char user_name[]="user";
	char password[]="password";

	connect_pkt.pkt_data.connect.connect_flags.flags.username_flag=1;
	connect_pkt.pkt_data.connect.connect_flags.flags.password_flag=1;

	connect_pkt.pkt_data.connect.user_name.str=(uint8_t*)user_name;
	connect_pkt.pkt_data.connect.user_name.len=strlen(user_name);

	connect_pkt.pkt_data.connect.password.str=(uint8_t*)password;
	connect_pkt.pkt_data.connect.password.len=strlen(password);
	*/

	/*
	//will message and topic configuration

	char will_topic[]="will/yamc";
	char will_message[]="Goodbye cruel world!";

	connect_pkt.pkt_data.connect.connect_flags.flags.will_flag=1;
	connect_pkt.pkt_data.connect.connect_flags.flags.will_remain=0;
	connect_pkt.pkt_data.connect.connect_flags.flags.will_qos=0;

	connect_pkt.pkt_data.connect.will_topic.str=(uint8_t*)will_topic;
	connect_pkt.pkt_data.connect.will_topic.len=strlen(will_topic);

	connect_pkt.pkt_data.connect.will_message.str=(uint8_t*)will_message;
	connect_pkt.pkt_data.connect.will_message.len=strlen(will_message);
	*/

	yamc_retcode_t ret = yamc_send_pkt(&yamc_instance, &connect_pkt);
	if (ret != YAMC_RET_SUCCESS)
	{
		printf("Error sending connect packet: %u\n", ret);
		exit(-1);
	}

	// send MQTT publish packet
	yamc_mqtt_pkt_data_t publish_pkt;
	memset(&publish_pkt, 0, sizeof(yamc_mqtt_pkt_data_t));

	char topic_name[] = "test/hello";
	char payload[]	= "Hello World!";

	publish_pkt.pkt_type						= YAMC_PKT_PUBLISH;
	publish_pkt.flags.QOS						= 1;
	publish_pkt.pkt_data.publish.packet_id		= 1337;
	publish_pkt.pkt_data.publish.topic_name.str = (uint8_t*)topic_name;
	publish_pkt.pkt_data.publish.topic_name.len = strlen(topic_name);

	publish_pkt.pkt_data.publish.payload.p_data   = (uint8_t*)payload;
	publish_pkt.pkt_data.publish.payload.data_len = strlen(payload);

	ret = yamc_send_pkt(&yamc_instance, &publish_pkt);
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

	ret = yamc_send_pkt(&yamc_instance, &subscribe_pkt);
	if (ret != YAMC_RET_SUCCESS)
	{
		printf("Error sending subscribe packet: %u\n", ret);
		exit(-1);
	}

	// unsubscribe topic 'test/#'
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

	ret = yamc_send_pkt(&yamc_instance, &unsubscribe_pkt);
	if (ret != YAMC_RET_SUCCESS)
	{
		printf("Error sending unsubscribe packet: %u\n", ret);
		exit(-1);
	}
	*/

	// repeatedly send ping request to keep connection alive
	while (!exit_now)
	{
		yamc_mqtt_pkt_data_t pingreq_pkt;
		memset(&pingreq_pkt, 0, sizeof(yamc_mqtt_pkt_data_t));

		pingreq_pkt.pkt_type = YAMC_PKT_PINGREQ;

		ret = yamc_send_pkt(&yamc_instance, &pingreq_pkt);
		if (ret != YAMC_RET_SUCCESS)
		{
			printf("Error sending pingreq packet: %u\n", ret);
			exit(-1);
		}

		sleep(25);
	}

	// cleanup
	close(server_socket);

	return 0;
}
