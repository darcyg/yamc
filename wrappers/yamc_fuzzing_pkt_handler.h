/*
 * YAMC - Yet Another MQTT Client library
 *
 * yamc_fuzzing_pkt_handler.c - overwrite parsed data to detect memory allocation problems
 *
 * Author: Michal Lower <keton22@gmail.com>
 *
 * All rights reserved 2017
 *
 */

#ifndef __YAMC_FUZZING_PKT_HANDLER_H__
#define __YAMC_FUZZING_PKT_HANDLER_H__

#include "yamc.h"

void yamc_fuzzing_pkt_handler_main(yamc_instance_t* const p_instance, const yamc_mqtt_pkt_data_t* const p_pkt_data);

#endif
