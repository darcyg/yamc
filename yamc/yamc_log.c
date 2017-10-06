/*
 * YAMC - Yet Another MQTT Client library
 *
 * yamc_log.c - debug log module
 *
 * Author: Michal Lower <keton22@gmail.com>
 *
 * All rights reserved 2017
 *
 */

#include "yamc_log.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "yamc.h"

inline const char* yamc_mqtt_pkt_type_to_str(yamc_pkt_type_t pkt_type)
{
	switch (pkt_type)
	{
		case YAMC_PKT_CONNECT:
			return "CONNECT";

		case YAMC_PKT_CONNACK:
			return "CONNACK";

		case YAMC_PKT_PUBLISH:
			return "PUBLISH";

		case YAMC_PKT_PUBACK:
			return "PUBACK";

		case YAMC_PKT_PUBREC:
			return "PUBREC";

		case YAMC_PKT_PUBREL:
			return "PUBREL";

		case YAMC_PKT_PUBCOMP:
			return "PUBCOMP";

		case YAMC_PKT_SUBSCRIBE:
			return "SUBSCRIBE";

		case YAMC_PKT_SUBACK:
			return "SUBACK";

		case YAMC_PKT_UNSUBSCRIBE:
			return "UNSUBSCRIBE";

		case YAMC_PKT_UNSUBACK:
			return "UNSUBACK";

		case YAMC_PKT_PINGREQ:
			return "PINGREQ";

		case YAMC_PKT_PINGRESP:
			return "PINGRESP";

		case YAMC_PKT_DISCONNECT:
			return "DISCONNECT";

		default:
			return "Unknown packet type";
	}
}

// functions below are used only in debug mode
#ifdef YAMC_DEBUG

void yamc_log_hex(const uint8_t* const p_buff, const uint32_t buff_len)
{
	YAMC_ASSERT(p_buff != NULL);

	for (uint32_t i = 0; i < buff_len; i++)
		YAMC_LOG_DEBUG("%02X ", p_buff[i]);
	YAMC_LOG_DEBUG("\n");
}

void yamc_log_raw_pkt(const yamc_instance_t* const p_instance)
{
	YAMC_ASSERT(p_instance != NULL);

	YAMC_LOG_DEBUG("> %s - %d bytes: ", yamc_mqtt_pkt_type_to_str(p_instance->rx_pkt.fixed_hdr.pkt_type.flags.type),
				   p_instance->rx_pkt.fixed_hdr.remaining_len.decoded_val);

	yamc_log_hex(p_instance->rx_pkt.var_data.data, p_instance->rx_pkt.var_data.pos);
}

#endif /* ifdef YAMC_DEBUG */
