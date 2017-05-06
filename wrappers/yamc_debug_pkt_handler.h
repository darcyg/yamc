/*
 * YAMC - Yet Another MQTT Client library
 *
 * yamc_debug_pkt_handler.h - Example code for yamc 'new packet' event handlers.
 *
 * Author: Michal Lower <keton22@gmail.com>
 *
 * All rights reserved 2017
 *
 */

#ifndef __YAMC_DEBUG_PKT_HANDLER_H__
#define __YAMC_DEBUG_PKT_HANDLER_H__

#include "yamc.h"

void yamc_debug_pkt_handler_main(yamc_instance_t* const p_instance, const yamc_mqtt_pkt_data_t* const p_pkt_data);

#endif
