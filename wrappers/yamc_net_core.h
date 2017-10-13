/*
 * YAMC - Yet Another MQTT Client library
 *
 * yamc_net_core.h - handles networking and timers on Unix platform
 *
 * Author: Michal Lower <https://github.com/keton>
 * 
 * Licensed under MIT License (see LICENSE file in main repo directory)
 * 
 */

#include <pthread.h>
#include <time.h>
#include "yamc.h"

typedef struct 
{
	yamc_instance_t instance;
	volatile uint8_t exit_now;
	int server_socket;
	pthread_t rx_tid;
	timer_t timeout_timer;


} yamc_net_core_t;

void yamc_net_core_connect(yamc_net_core_t* const p_net_core, char* hostname, int port, yamc_pkt_handler_t pkt_handler);
 
bool yamc_net_core_should_exit(yamc_net_core_t* const p_net_core);

void yamc_net_core_disconnect(yamc_net_core_t* const p_net_core);

