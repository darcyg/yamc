/*
 * YAMC - Yet Another MQTT Client library
 *
 * yamc_runner_pkt_handlers.c - Example code for yamc 'new packet' event handlers.
 *
 * Author: Michal Lower <keton22@gmail.com>
 *
 * All rights reserved 2017
 *
 */

#include "yamc.h"
#include "yamc_log.h"
#include "yamc_runner_pkt_handlers.h"

static inline void yamc_handle_connack(const yamc_instance_t* const p_instance, const yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_ASSERT(p_instance!=NULL);
	YAMC_ASSERT(p_pkt_data!=NULL);

	YAMC_LOG_DEBUG("CONNACK: session_present: %d, ret_code: 0x%02X\n",
			p_pkt_data->pkt_data.connack.ack_flags.flags.session_present, p_pkt_data->pkt_data.connack.return_code);

}

static inline void yamc_handle_publish(const yamc_instance_t* const p_instance, const yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_ASSERT(p_instance!=NULL);
	YAMC_ASSERT(p_pkt_data!=NULL);

	const yamc_mqtt_pkt_publish_t* const p_data=&p_pkt_data->pkt_data.publish;

	YAMC_LOG_DEBUG("PUBLISH topic: \"%.*s\" msg: \"%.*s\"\n",
			p_data->topic_name.len, p_data->topic_name.str, p_data->payload.data_len, p_data->payload.p_data);

}

static inline void yamc_handle_pub_x(const yamc_instance_t* const p_instance, const yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_ASSERT(p_instance!=NULL);
	YAMC_ASSERT(p_pkt_data!=NULL);

	const yamc_mqtt_pkt_generic_pubx_t* p_dest_pkt;

	switch(p_pkt_data->pkt_type)
	{
	case YAMC_PKT_PUBACK:
		p_dest_pkt=&p_pkt_data->pkt_data.puback;
		break;

	case YAMC_PKT_PUBREC:
		p_dest_pkt=&p_pkt_data->pkt_data.pubrec;
		break;

	case YAMC_PKT_PUBREL:
		p_dest_pkt=&p_pkt_data->pkt_data.pubrel;
		break;

	case YAMC_PKT_PUBCOMP:
		p_dest_pkt=&p_pkt_data->pkt_data.pubcomp;
		break;

	case YAMC_PKT_UNSUBACK:
		p_dest_pkt=&p_pkt_data->pkt_data.unsuback;
		break;

	default:
		return;
	}

	YAMC_LOG_DEBUG("%s: pkt_id: %d\n", yamc_mqtt_pkt_type_to_str(p_pkt_data->pkt_type), p_dest_pkt->packet_id);
}

static inline void yamc_handle_suback(const yamc_instance_t* const p_instance, const yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_ASSERT(p_instance!=NULL);
	YAMC_ASSERT(p_pkt_data!=NULL);

	const yamc_mqtt_pkt_suback_t* const p_data=&p_pkt_data->pkt_data.suback;

	YAMC_LOG_DEBUG("SUBACK: pkt_id:%d %d return codes in payload\n", p_data->pkt_id, p_data->payload.retcodes_len);

	for (uint16_t i=0;i<p_data->payload.retcodes_len; i++)
	{
		YAMC_LOG_DEBUG("\t Topic: %u, retcode: 0x%02X\n", i, p_data->payload.p_retcodes[i]);
	}

}

static inline void yamc_handle_pingresp(const yamc_instance_t* const p_instance, const yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_ASSERT(p_instance!=NULL);
	YAMC_ASSERT(p_pkt_data!=NULL);

	YAMC_LOG_DEBUG("PINGRESP\n");

}


inline void yamc_debug_pkt_handler_main(yamc_instance_t* const p_instance, const yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_ASSERT(p_instance!=NULL);
	YAMC_ASSERT(p_pkt_data!=NULL);

	//decode data according to packet type
	switch(p_instance->rx_pkt.fixed_hdr.pkt_type.flags.type)
	{
	case YAMC_PKT_CONNACK:
		yamc_handle_connack(p_instance, p_pkt_data);
		break;

	case YAMC_PKT_PUBLISH:
		yamc_handle_publish(p_instance, p_pkt_data);
		break;

	case YAMC_PKT_PUBACK:
	case YAMC_PKT_PUBREC:
	case YAMC_PKT_PUBREL:
	case YAMC_PKT_PUBCOMP:
	case YAMC_PKT_UNSUBACK:
		yamc_handle_pub_x(p_instance, p_pkt_data);
		break;

	case YAMC_PKT_SUBACK:
		yamc_handle_suback(p_instance, p_pkt_data);
		break;

	case YAMC_PKT_PINGRESP:
		yamc_handle_pingresp(p_instance, p_pkt_data);
		break;

	default:
		YAMC_LOG_ERROR("Unknown packet type %d\n", p_pkt_data->pkt_type);
		break;
	}

}
