/*
 * YAMC - Yet Another MQTT Client library
 *
 * yamc_runner_socket.c - Pass stdin to yamc_parse_buf(). Allows fuzzing of yamc.
 *
 * Author: Michal Lower <keton22@gmail.com>
 *
 * All rights reserved 2017
 *
 */

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "yamc.h"
#include "yamc_port.h"

// example user defined packet handlers, overwrite parsed data to detect memory allocation problems
#include "yamc_fuzzing_pkt_handler.h"

// yamc state instance
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
static void timeout_pat(void* p_ctx)
{
	YAMC_UNUSED_PARAMETER(p_ctx);
	
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
static void timeout_stop(void* p_ctx)
{
	YAMC_UNUSED_PARAMETER(p_ctx);

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
static yamc_retcode_t socket_write_buff(void* p_ctx, const uint8_t* const buff, uint32_t len)
{
	YAMC_UNUSED_PARAMETER(p_ctx);
	
	ssize_t n = write(STDOUT_FILENO, buff, len);
	if (n < 0)
	{
		YAMC_ERROR_PRINTF("ERROR writing to socket:%s\n", strerror(n));
		return YAMC_RET_INVALID_STATE;
	}
	return YAMC_RET_SUCCESS;
}

static void disconnect_handler(void* p_ctx)
{
	YAMC_UNUSED_PARAMETER(p_ctx);
	
	YAMC_ERROR_PRINTF("yamc requested to drop connection!\n");
	exit(-1);
}

int main(void)
{
	setup_timer();

	yamc_handler_cfg_t handler_cfg = {.disconnect   = disconnect_handler,
									  .write		= socket_write_buff,
									  .timeout_pat  = timeout_pat,
									  .timeout_stop = timeout_stop,
									  .pkt_handler  = yamc_fuzzing_pkt_handler_main};

	yamc_init(&yamc_instance, &handler_cfg);

	// enable all packet handlers for fuzzing
	yamc_instance.parser_enables.CONNACK  = true;
	yamc_instance.parser_enables.PUBLISH  = true;
	yamc_instance.parser_enables.PUBACK   = true;
	yamc_instance.parser_enables.PINGRESP = true;
	yamc_instance.parser_enables.SUBACK   = true;
	yamc_instance.parser_enables.PUBCOMP  = true;
	yamc_instance.parser_enables.PUBREC   = true;
	yamc_instance.parser_enables.PUBREL   = true;
	yamc_instance.parser_enables.UNSUBACK = true;

	// buffer for incoming data
	uint8_t rx_buff[10];

	// how many bytes were received in single read operation or read() error code
	int rx_bytes = 0;

	// zero rx buffer
	memset(rx_buff, 0, sizeof(rx_buff));

	// read stdin and pass it to yamc_parse_buff()
	while ((rx_bytes = read(STDIN_FILENO, rx_buff, sizeof(rx_buff))) > 0)
		if (rx_bytes > 0) yamc_parse_buff(&yamc_instance, rx_buff, rx_bytes);

	exit(0);
}
