/*
 * YAMC - Yet Another MQTT Client library
 *
 * yamc_net_core.c - handles networking and timers on Unix platform
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

#include "yamc_net_core.h"
#include "yamc.h"
#include "yamc_port.h"

// timeout timer settings
#define YAMC_TIMEOUT_S 30  // seconds
#define YAMC_TIMEOUT_NS 0  // nanoseconds

//global exit flag. If set all rx threads will exit
static volatile bool global_exit_now = false;

// timeout timer signal handler
static void yamc_net_core_timeout_handler(sigval_t sigval)
{
	YAMC_UNUSED_PARAMETER(sigval);

	YAMC_ERROR_PRINTF("Timeout!\n");
	fflush(stderr);

	exit(-1);
}

// start/prolong timeout timer wrapper
static void yamc_net_core_timeout_pat(void* p_ctx)
{
	YAMC_ASSERT(p_ctx != NULL);
	yamc_net_core_t* const p_net_core = (yamc_net_core_t*)p_ctx;

	struct itimerspec its;
	memset(&its, 0, sizeof(struct itimerspec));
	its.it_value.tv_sec  = YAMC_TIMEOUT_S;
	its.it_value.tv_nsec = YAMC_TIMEOUT_NS;

	int err_code = timer_settime(p_net_core->timeout_timer, 0, &its, NULL);
	if (err_code < 0)
	{
		YAMC_ERROR_PRINTF("Timer error\n");
		exit(-1);
	}
}

// stop timeout timer
static void yamc_net_core_timeout_stop(void* p_ctx)
{
	YAMC_ASSERT(p_ctx != NULL);
	yamc_net_core_t* const p_net_core = (yamc_net_core_t*)p_ctx;

	struct itimerspec its;
	memset(&its, 0, sizeof(struct itimerspec));

	int err_code = timer_settime(p_net_core->timeout_timer, 0, &its, NULL);
	if (err_code < 0)
	{
		YAMC_ERROR_PRINTF("Timer error\n");
		exit(-1);
	}
}

// timeout timer setup
static void yamc_net_core_setup_timer(yamc_net_core_t* const p_net_core)
{
	YAMC_ASSERT(p_net_core != NULL);

	int err_code = -1;

	struct sigevent sev;

	memset(&sev, 0, sizeof(struct sigevent));

	sev.sigev_signo			  = SIGRTMIN;						// signal type
	sev.sigev_notify		  = SIGEV_THREAD;					// call timeout handler as if it was starting a new thread
	sev.sigev_notify_function = yamc_net_core_timeout_handler;  // set timeout handler

	if ((err_code = timer_create(CLOCK_REALTIME, &sev, &p_net_core->timeout_timer)) < 0)
	{
		YAMC_ERROR_PRINTF("ERROR setting up timeout timer: %s\n", strerror(err_code));
		exit(-1);
	}
}

// write to socket wrapper
static yamc_retcode_t yamc_net_core_write(void* p_ctx, const uint8_t* const buff, uint32_t len)
{
	YAMC_ASSERT(p_ctx != NULL);

	yamc_net_core_t* p_net_core = (yamc_net_core_t*)p_ctx;

	ssize_t n = write(p_net_core->server_socket, buff, len);
	if (n < 0)
	{
		YAMC_ERROR_PRINTF("Error writing to socket:%s\n", strerror(n));
		return YAMC_RET_INVALID_STATE;
	}
	if (n != len)
	{
		YAMC_ERROR_PRINTF("Warning: Incomplete socket write!");
	}
	return YAMC_RET_SUCCESS;
}

static void yamc_net_core_disconnect_handler(void* p_ctx)
{
	YAMC_ASSERT(p_ctx != NULL);
	yamc_net_core_t* p_net_core = (yamc_net_core_t*)p_ctx;

	YAMC_ERROR_PRINTF("yamc requested to drop connection!\n");

	close(p_net_core->server_socket);
	p_net_core->exit_now = true;
}

// receive data from socket thread
static void* yamc_net_core_rx_thread(void* p_ctx)
{
	YAMC_ASSERT(p_ctx != NULL);

	yamc_net_core_t* p_net_core = (yamc_net_core_t*)p_ctx;

	// buffer for incoming data
	uint8_t rx_buff[10];

	// how many bytes were received in single read operation or read() error code
	int rx_bytes = 0;

	do
	{
		// zero rx buffer
		memset(rx_buff, 0, sizeof(rx_buff));

		rx_bytes = read(p_net_core->server_socket, rx_buff, sizeof(rx_buff));

		// there was error code thrown by read()
		if (rx_bytes < 0)
		{
			YAMC_ERROR_PRINTF("TCP read() error: %s\n", strerror(rx_bytes));
			p_net_core->exit_now = true;
			pthread_exit(&rx_bytes);
		}

		// process buffer here
		if (rx_bytes > 0) yamc_parse_buff(&p_net_core->instance, rx_buff, rx_bytes);

	} while (rx_bytes > 0);

	p_net_core->exit_now = true;
	return NULL;
}

// connect socket to specified host and port
static void yamc_net_core_setup_socket(yamc_net_core_t* const p_net_core, char* hostname, int portno)
{
	// server socket address struct
	struct sockaddr_in serv_addr;

	// host data struct pointer
	struct hostent* server;

	int err_code = 0;

	memset(&serv_addr, 0, sizeof(serv_addr));

	// create socket
	p_net_core->server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (p_net_core->server_socket < 0) YAMC_ERROR_PRINTF("ERROR opening socket\n");

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
	if ((err_code = connect(p_net_core->server_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0)
	{
		YAMC_ERROR_PRINTF("ERROR connecting: %s\n", strerror(err_code));
		exit(0);
	}
}

//handle shutdown (i.e. Ctrl+C) in graceful fashion
static void yamc_net_core_sigint_handler(int signal)
{
	YAMC_UNUSED_PARAMETER(signal);

	//signal that we should go now...
	global_exit_now = true;
}

//setup signal handler for Crtl+C
static void yamc_net_core_setup_sigint_handler(void)
{
	struct sigaction sigint_action;
	memset(&sigint_action, 0, sizeof(struct sigaction));
	sigint_action.sa_handler = yamc_net_core_sigint_handler;
	sigemptyset(&sigint_action.sa_mask);
	sigint_action.sa_flags = 0;
	sigaction(SIGINT, &sigint_action, NULL);
	sigaction(SIGHUP, &sigint_action, NULL);
	sigaction(SIGTERM, &sigint_action, NULL);
}

void yamc_net_core_connect(yamc_net_core_t* const p_net_core, char* hostname, int port, yamc_pkt_handler_t pkt_handler)
{
	YAMC_ASSERT(p_net_core != NULL);
	YAMC_ASSERT(pkt_handler != NULL);

	memset(p_net_core, 0, sizeof(yamc_net_core_t));

	// setup timeout timer
	yamc_net_core_setup_timer(p_net_core);

	// setup socket and connect to server
	yamc_net_core_setup_socket(p_net_core, hostname, port);

	//setup disconnect on signal
	yamc_net_core_setup_sigint_handler();

	yamc_handler_cfg_t handler_cfg = {.disconnect	= yamc_net_core_disconnect_handler,
									  .write		 = yamc_net_core_write,
									  .timeout_pat   = yamc_net_core_timeout_pat,
									  .timeout_stop  = yamc_net_core_timeout_stop,
									  .pkt_handler   = pkt_handler,
									  .p_handler_ctx = p_net_core};

	yamc_init(&p_net_core->instance, &handler_cfg);

	// create thread
	pthread_create(&p_net_core->rx_tid, NULL, yamc_net_core_rx_thread, p_net_core);
}

bool yamc_net_core_should_exit(yamc_net_core_t* const p_net_core)
{
	YAMC_ASSERT(p_net_core != NULL);

	return p_net_core->exit_now || global_exit_now;
}

void yamc_net_core_disconnect(yamc_net_core_t* const p_net_core)
{
	YAMC_ASSERT(p_net_core != NULL);

	//signal rx thread to exit
	p_net_core->exit_now = true;

	// send MQTT disconnect packet
	yamc_retcode_t ret = yamc_disconnect(&p_net_core->instance);
	if (ret != YAMC_RET_SUCCESS)
	{
		printf("Error sending disconnect packet: %u\n", ret);
		exit(-1);
	}

	close(p_net_core->server_socket);
}
