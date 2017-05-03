
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

	YAMC_LOG_DEBUG("Stub: yamc_handle_pub_x NOT implemented!!\n");

}

static inline void yamc_handle_suback(const yamc_instance_t* const p_instance, const yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_ASSERT(p_instance!=NULL);
	YAMC_ASSERT(p_pkt_data!=NULL);

	YAMC_LOG_DEBUG("Stub: yamc_handle_suback NOT implemented!!\n");

}

static inline void yamc_handle_unsuback(const yamc_instance_t* const p_instance, const yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_ASSERT(p_instance!=NULL);
	YAMC_ASSERT(p_pkt_data!=NULL);

	YAMC_LOG_DEBUG("Stub: yamc_handle_unsuback NOT implemented!!\n");

}

static inline void yamc_handle_pingresp(const yamc_instance_t* const p_instance, const yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_ASSERT(p_instance!=NULL);
	YAMC_ASSERT(p_pkt_data!=NULL);

	YAMC_LOG_DEBUG("Stub: yamc_handle_pingresp NOT implemented!!\n");

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
		yamc_handle_pub_x(p_instance, p_pkt_data);
		break;

	case YAMC_PKT_SUBACK:
		yamc_handle_suback(p_instance, p_pkt_data);
		break;

	case YAMC_PKT_UNSUBACK:
		yamc_handle_unsuback(p_instance, p_pkt_data);
		break;

	case YAMC_PKT_PINGRESP:
		yamc_handle_pingresp(p_instance, p_pkt_data);
		break;

	default:
		YAMC_LOG_ERROR("Unknown packet type %d\n", p_pkt_data->pkt_type);
		break;
	}

}
