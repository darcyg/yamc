/*
 * YAMC - Yet Another MQTT Client library
 *
 * yamc_log.h - debug log module
 *
 * Author: Michal Lower <keton22@gmail.com>
 *
 * All rights reserved 2017
 *
 */

#ifndef __YAMC_LOG_H__
#define __YAMC_LOG_H__

#include "yamc_port.h"

const char* yamc_mqtt_pkt_type_to_str(yamc_pkt_type_t pkt_type);

#ifdef YAMC_DEBUG

///debug log macro
#define YAMC_LOG_DEBUG(...) YAMC_DEBUG_PRINTF(__VA_ARGS__)

///error log macro
#define YAMC_LOG_ERROR(...) YAMC_ERROR_PRINTF(__VA_ARGS__)

void yamc_log_hex(const uint8_t* const p_buff, const uint32_t buff_len);
void yamc_log_raw_pkt(const yamc_instance_t* const p_instance);

#else /* YAMC_DEBUG not defined */

#define YAMC_LOG_DEBUG(...)
#define YAMC_LOG_ERROR(...)
#define yamc_log_hex(...)
#define yamc_log_raw_pkt(...)

#endif /* YAMC_DEBUG */

#endif /* YAMC_LOG_H_ */
