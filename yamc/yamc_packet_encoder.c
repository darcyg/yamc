/*
 * YAMC - Yet Another MQTT Client library
 *
 * yamc_packet_encoder.c - Encodes and sends packets.
 *
 * Author: Michal Lower <keton22@gmail.com>
 *
 * All rights reserved 2017
 *
 */

#include <stdbool.h>
#include <string.h>
#include "yamc.h"
#include "yamc_log.h"

typedef union {
	uint16_t val;
	uint8_t  raw[2];

} yamc_mqtt_word_t;

static inline void yamc_encode_mqtt_word(const uint16_t val, yamc_mqtt_word_t* const p_word)
{
	YAMC_ASSERT(p_word != NULL);

	p_word->raw[0] = (val >> 8) & 0xFF;
	p_word->raw[1] = val & 0xFF;
}

// encode 'remaining length' field in fixed header
static inline void yamc_encode_rem_length(uint32_t rem_length, yamc_mqtt_hdr_fixed_t* const p_fixed_hdr)
{
	YAMC_ASSERT(p_fixed_hdr != NULL);
	YAMC_ASSERT(rem_length < YAMC_MQTT_MAX_LEN);
	do
	{
		p_fixed_hdr->remaining_len.raw[p_fixed_hdr->remaining_len.raw_len] = rem_length % 128;
		rem_length														   = rem_length / 128;

		// if there are more data to encode, set the top bit of this byte
		if (rem_length > 0)
		{
			p_fixed_hdr->remaining_len.raw[p_fixed_hdr->remaining_len.raw_len] |= 128;
		}

		p_fixed_hdr->remaining_len.raw_len++;
	} while (rem_length > 0);
}

static inline bool yamc_is_mqtt_string_present(const yamc_mqtt_string* const p_mqtt_str)
{
	YAMC_ASSERT(p_mqtt_str != NULL);
	return p_mqtt_str->len > 0 && p_mqtt_str->str != NULL;
}

static inline uint16_t yamc_mqtt_string_raw_length(const yamc_mqtt_string* const p_mqtt_str)
{
	YAMC_ASSERT(p_mqtt_str != NULL);
	return (p_mqtt_str->len) ? p_mqtt_str->len + 2 : 0;
}

static inline yamc_retcode_t yamc_send_word(yamc_instance_t* const p_instance, const uint16_t word)
{
	YAMC_ASSERT(p_instance != NULL);

	yamc_mqtt_word_t mqtt_word = {0};
	yamc_encode_mqtt_word(word, &mqtt_word);

	return p_instance->handlers.write(mqtt_word.raw, 2);
}

static inline yamc_retcode_t yamc_send_str(yamc_instance_t* const p_instance, const yamc_mqtt_string* const p_mqtt_str)
{
	YAMC_ASSERT(p_instance != NULL);
	YAMC_ASSERT(p_mqtt_str != NULL);

	yamc_retcode_t ret = YAMC_RET_SUCCESS;

	ret = yamc_send_word(p_instance, p_mqtt_str->len);

	if (ret != YAMC_RET_SUCCESS) return ret;

	if (p_mqtt_str->len > 0)
	{
		return p_instance->handlers.write(p_mqtt_str->str, p_mqtt_str->len);
	}
	else
	{
		return ret;
	}
}

static inline yamc_retcode_t yamc_send_fixed_hdr(yamc_instance_t* const p_instance, const yamc_mqtt_hdr_fixed_t* const p_fixed_hdr)
{
	YAMC_ASSERT(p_instance != NULL);
	YAMC_ASSERT(p_fixed_hdr != NULL);

	uint8_t send_buff[YAMC_MQTT_REM_LEN_MAX + 1];
	memset(send_buff, 0, sizeof(send_buff));
	send_buff[0] = p_fixed_hdr->pkt_type.raw;
	memcpy(&send_buff[1], p_fixed_hdr->remaining_len.raw, p_fixed_hdr->remaining_len.raw_len);

	return p_instance->handlers.write(send_buff, p_fixed_hdr->remaining_len.raw_len + 1);
}

static inline yamc_retcode_t yamc_send_connect(yamc_instance_t* const p_instance, yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_ASSERT(p_instance != NULL);
	YAMC_ASSERT(p_pkt_data != NULL);

	yamc_mqtt_hdr_fixed_t fixed_hdr;
	memset(&fixed_hdr, 0, sizeof(yamc_mqtt_hdr_fixed_t));

	fixed_hdr.pkt_type.flags.type   = YAMC_PKT_CONNECT;
	fixed_hdr.pkt_type.flags.DUP	= p_pkt_data->flags.DUP;
	fixed_hdr.pkt_type.flags.QOS	= p_pkt_data->flags.QOS;
	fixed_hdr.pkt_type.flags.RETAIN = p_pkt_data->flags.RETAIN;

	/*
	 *
	 * mandatory fields:
	 *
	 * MQTT protocol magic + length: 6 bytes
	 * protocol level: 1 byte
	 * connect flags: 1 byte
	 * keep alive: 2 bytes
	*/
	uint32_t rem_len = 6 + 1 + 1 + 2;

	// TODO: handle special case when zero length client ID is supplied
	// for now client id must be always present
	if (!yamc_is_mqtt_string_present(&p_pkt_data->pkt_data.connect.client_id)) return YAMC_RET_INVALID_DATA;

	rem_len += yamc_mqtt_string_raw_length(&p_pkt_data->pkt_data.connect.client_id);

	// if will flag is set will message and will topic must be declared
	if (p_pkt_data->pkt_data.connect.connect_flags.flags.will_flag)
	{
		if (!yamc_is_mqtt_string_present(&p_pkt_data->pkt_data.connect.will_message) ||
			!yamc_is_mqtt_string_present(&p_pkt_data->pkt_data.connect.will_topic))
			return YAMC_RET_INVALID_DATA;

		rem_len += yamc_mqtt_string_raw_length(&p_pkt_data->pkt_data.connect.will_message);
		rem_len += yamc_mqtt_string_raw_length(&p_pkt_data->pkt_data.connect.will_topic);
	}

	if (p_pkt_data->pkt_data.connect.connect_flags.flags.username_flag &&
		!yamc_is_mqtt_string_present(&p_pkt_data->pkt_data.connect.user_name))
		return YAMC_RET_INVALID_DATA;

	rem_len += yamc_mqtt_string_raw_length(&p_pkt_data->pkt_data.connect.user_name);

	if (p_pkt_data->pkt_data.connect.connect_flags.flags.password_flag &&
		!yamc_is_mqtt_string_present(&p_pkt_data->pkt_data.connect.password))
		return YAMC_RET_INVALID_DATA;

	rem_len += yamc_mqtt_string_raw_length(&p_pkt_data->pkt_data.connect.password);

	// per spec if password is supplied user name must be present
	if (p_pkt_data->pkt_data.connect.connect_flags.flags.password_flag && !p_pkt_data->pkt_data.connect.connect_flags.flags.username_flag)
		return YAMC_RET_INVALID_DATA;

	// encode remaining length in packet header
	yamc_encode_rem_length(rem_len, &fixed_hdr);

	// send the data

	// send fixed header
	yamc_retcode_t ret = yamc_send_fixed_hdr(p_instance, &fixed_hdr);
	if (ret != YAMC_RET_SUCCESS) return ret;

	// send protocol magic string
	static const yamc_mqtt_string mqtt_magic = {.str = (uint8_t*)"MQTT", .len = 4};

	ret = yamc_send_str(p_instance, &mqtt_magic);
	if (ret != YAMC_RET_SUCCESS) return ret;

	// send protocol version = 4 and conect flags
	uint8_t protocol_flags[2] = {4, p_pkt_data->pkt_data.connect.connect_flags.raw};
	ret						  = p_instance->handlers.write(protocol_flags, 2);
	if (ret != YAMC_RET_SUCCESS) return ret;

	// send keepalive value
	ret = yamc_send_word(p_instance, p_pkt_data->pkt_data.connect.keepalive_timeout_s);
	if (ret != YAMC_RET_SUCCESS) return ret;

	// payload order: Client Identifier, Will Topic, Will Message, User Name, Password
	ret = yamc_send_str(p_instance, &p_pkt_data->pkt_data.connect.client_id);
	if (ret != YAMC_RET_SUCCESS) return ret;

	if (p_pkt_data->pkt_data.connect.connect_flags.flags.will_flag)
	{
		ret = yamc_send_str(p_instance, &p_pkt_data->pkt_data.connect.will_topic);
		if (ret != YAMC_RET_SUCCESS) return ret;
		ret = yamc_send_str(p_instance, &p_pkt_data->pkt_data.connect.will_message);
		if (ret != YAMC_RET_SUCCESS) return ret;
	}

	if (p_pkt_data->pkt_data.connect.connect_flags.flags.username_flag)
	{
		ret = yamc_send_str(p_instance, &p_pkt_data->pkt_data.connect.user_name);
		if (ret != YAMC_RET_SUCCESS) return ret;
	}

	if (p_pkt_data->pkt_data.connect.connect_flags.flags.password_flag)
	{
		ret = yamc_send_str(p_instance, &p_pkt_data->pkt_data.connect.password);
		if (ret != YAMC_RET_SUCCESS) return ret;
	}

	return YAMC_RET_SUCCESS;
}

static inline yamc_retcode_t yamc_send_publish(yamc_instance_t* const p_instance, yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_ASSERT(p_instance != NULL);
	YAMC_ASSERT(p_pkt_data != NULL);

	yamc_mqtt_hdr_fixed_t fixed_hdr;
	memset(&fixed_hdr, 0, sizeof(yamc_mqtt_hdr_fixed_t));

	fixed_hdr.pkt_type.flags.type   = YAMC_PKT_PUBLISH;
	fixed_hdr.pkt_type.flags.DUP	= p_pkt_data->flags.DUP;
	fixed_hdr.pkt_type.flags.QOS	= p_pkt_data->flags.QOS;
	fixed_hdr.pkt_type.flags.RETAIN = p_pkt_data->flags.RETAIN;

	/*
	 *
	 * mandatory fields:
	 *
	 * Packet identifier: 2 bytes when qos>0
	*/
	uint32_t rem_len = 0;

	if (p_pkt_data->flags.QOS > 0) rem_len += 2;

	if (!yamc_is_mqtt_string_present(&p_pkt_data->pkt_data.publish.topic_name))
	{
		return YAMC_RET_INVALID_DATA;
	}

	rem_len += yamc_mqtt_string_raw_length(&p_pkt_data->pkt_data.publish.topic_name);

	// publish data re application specific (not an MQTT string), it is valid for publish to contain empty payload
	if (p_pkt_data->pkt_data.publish.payload.p_data == NULL && p_pkt_data->pkt_data.publish.payload.data_len > 0)
	{
		return YAMC_RET_INVALID_DATA;
	}

	rem_len += p_pkt_data->pkt_data.publish.payload.data_len;

	// encode remaining length in packet header
	yamc_encode_rem_length(rem_len, &fixed_hdr);

	// send the data

	// send fixed header
	yamc_retcode_t ret = yamc_send_fixed_hdr(p_instance, &fixed_hdr);
	if (ret != YAMC_RET_SUCCESS) return ret;

	// send publish topic
	ret = yamc_send_str(p_instance, &p_pkt_data->pkt_data.publish.topic_name);
	if (ret != YAMC_RET_SUCCESS) return ret;

	// send packet id
	if (p_pkt_data->flags.QOS > 0)
	{
		ret = yamc_send_word(p_instance, p_pkt_data->pkt_data.publish.packet_id);
		if (ret != YAMC_RET_SUCCESS) return ret;
	}

	// send payload
	if (p_pkt_data->pkt_data.publish.payload.data_len > 0)
	{
		ret = p_instance->handlers.write(p_pkt_data->pkt_data.publish.payload.p_data, p_pkt_data->pkt_data.publish.payload.data_len);
		if (ret != YAMC_RET_SUCCESS) return ret;
	}

	return YAMC_RET_SUCCESS;
}

static inline yamc_retcode_t yamc_send_subscribe(yamc_instance_t* const p_instance, yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_ASSERT(p_instance != NULL);
	YAMC_ASSERT(p_pkt_data != NULL);

	yamc_mqtt_hdr_fixed_t fixed_hdr;
	memset(&fixed_hdr, 0, sizeof(yamc_mqtt_hdr_fixed_t));

	// subscribe packet header has specific format
	fixed_hdr.pkt_type.raw = YAMC_PKT_SUBSCRIBE << 4 | 2;

	/*
	 *
	 * mandatory fields:
	 *
	 * Packet identifier: 2 bytes
	*/
	uint32_t rem_len = 2;

	if (p_pkt_data->pkt_data.subscribe.payload.topics_len == 0 || p_pkt_data->pkt_data.subscribe.payload.p_topics == NULL)
		return YAMC_RET_INVALID_DATA;

	for (uint16_t i = 0; i < p_pkt_data->pkt_data.subscribe.payload.topics_len; i++)
	{
		uint16_t topic_len = yamc_mqtt_string_raw_length(&p_pkt_data->pkt_data.subscribe.payload.p_topics[i].topic_name);

		// zero length subscription topic is illegal
		if (!topic_len) return YAMC_RET_INVALID_DATA;

		//+1 because each topic has corresponding qos byte
		rem_len += topic_len + 1;
	}

	// encode remaining length in packet header
	yamc_encode_rem_length(rem_len, &fixed_hdr);

	// send the data

	// send fixed header
	yamc_retcode_t ret = yamc_send_fixed_hdr(p_instance, &fixed_hdr);
	if (ret != YAMC_RET_SUCCESS) return ret;

	ret = yamc_send_word(p_instance, p_pkt_data->pkt_data.subscribe.pkt_id);
	if (ret != YAMC_RET_SUCCESS) return ret;

	for (uint16_t i = 0; i < p_pkt_data->pkt_data.subscribe.payload.topics_len; i++)
	{
		ret = yamc_send_str(p_instance, &p_pkt_data->pkt_data.subscribe.payload.p_topics[i].topic_name);
		if (ret != YAMC_RET_SUCCESS) return ret;

		ret = p_instance->handlers.write((uint8_t*)&p_pkt_data->pkt_data.subscribe.payload.p_topics[i].qos, 1);
		if (ret != YAMC_RET_SUCCESS) return ret;
	}

	return YAMC_RET_SUCCESS;
}

static inline yamc_retcode_t yamc_send_unsubscribe(yamc_instance_t* const p_instance, yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_ASSERT(p_instance != NULL);
	YAMC_ASSERT(p_pkt_data != NULL);

	yamc_mqtt_hdr_fixed_t fixed_hdr;
	memset(&fixed_hdr, 0, sizeof(yamc_mqtt_hdr_fixed_t));

	// unsubscribe packet header has specific format
	fixed_hdr.pkt_type.raw = YAMC_PKT_UNSUBSCRIBE << 4 | 2;

	/*
	 *
	 * mandatory fields:
	 *
	 * Packet identifier: 2 bytes
	*/
	uint32_t rem_len = 2;

	if (p_pkt_data->pkt_data.unsubscribe.payload.topics_len == 0 || p_pkt_data->pkt_data.unsubscribe.payload.p_topics == NULL)
		return YAMC_RET_INVALID_DATA;

	for (uint16_t i = 0; i < p_pkt_data->pkt_data.unsubscribe.payload.topics_len; i++)
	{
		uint16_t topic_len = yamc_mqtt_string_raw_length(&p_pkt_data->pkt_data.unsubscribe.payload.p_topics[i]);

		// zero length subscription topic is illegal
		if (!topic_len) return YAMC_RET_INVALID_DATA;

		//+1 because each topic has corresponding qos byte
		rem_len += topic_len;
	}

	// encode remaining length in packet header
	yamc_encode_rem_length(rem_len, &fixed_hdr);

	// send the data

	// send fixed header
	yamc_retcode_t ret = yamc_send_fixed_hdr(p_instance, &fixed_hdr);
	if (ret != YAMC_RET_SUCCESS) return ret;

	ret = yamc_send_word(p_instance, p_pkt_data->pkt_data.unsubscribe.pkt_id);
	if (ret != YAMC_RET_SUCCESS) return ret;

	for (uint16_t i = 0; i < p_pkt_data->pkt_data.unsubscribe.payload.topics_len; i++)
	{
		ret = yamc_send_str(p_instance, &p_pkt_data->pkt_data.unsubscribe.payload.p_topics[i]);
		if (ret != YAMC_RET_SUCCESS) return ret;
	}

	return YAMC_RET_SUCCESS;
}

//send packet that contains only fixed header (disconnect, pingreq, pingresp...)
static inline yamc_retcode_t yamc_send_fixed_hdr_only_pkt(yamc_instance_t* const p_instance, yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_ASSERT(p_instance != NULL);
	YAMC_ASSERT(p_pkt_data != NULL);

	yamc_mqtt_hdr_fixed_t fixed_hdr;
	memset(&fixed_hdr, 0, sizeof(yamc_mqtt_hdr_fixed_t));

	fixed_hdr.pkt_type.raw = p_pkt_data->pkt_type << 4;

	//disconnect packet has no data in it
	uint32_t rem_len = 0;

	// encode remaining length in packet header
	yamc_encode_rem_length(rem_len, &fixed_hdr);

	// send the data
	return yamc_send_fixed_hdr(p_instance, &fixed_hdr);
}

static inline yamc_retcode_t yamc_send_pub_x(yamc_instance_t* const p_instance, yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_ASSERT(p_instance != NULL);
	YAMC_ASSERT(p_pkt_data != NULL);

	yamc_mqtt_hdr_fixed_t fixed_hdr;
	memset(&fixed_hdr, 0, sizeof(yamc_mqtt_hdr_fixed_t));

	fixed_hdr.pkt_type.raw = p_pkt_data->pkt_type << 4;

	/*
	 *
	 * mandatory fields:
	 *
	 * Packet identifier: 2 bytes
	*/
	uint32_t rem_len = 2;

	// encode remaining length in packet header
	yamc_encode_rem_length(rem_len, &fixed_hdr);

	// send the data

	// send fixed header
	yamc_retcode_t ret = yamc_send_fixed_hdr(p_instance, &fixed_hdr);
	if (ret != YAMC_RET_SUCCESS) return ret;

	uint16_t* p_pkt_id=NULL;

	switch (p_pkt_data->pkt_type)
	{
		case YAMC_PKT_PUBACK:
			p_pkt_id = &p_pkt_data->pkt_data.puback.packet_id;
			break;

		case YAMC_PKT_PUBREL:
			p_pkt_id = &p_pkt_data->pkt_data.pubrel.packet_id;
			break;

		case YAMC_PKT_PUBREC:
			p_pkt_id = &p_pkt_data->pkt_data.pubrec.packet_id;
			break;

		case YAMC_PKT_PUBCOMP:
			p_pkt_id = &p_pkt_data->pkt_data.pubcomp.packet_id;
			break;

		default:
			return YAMC_RET_INVALID_DATA;
	}

	ret = yamc_send_word(p_instance, *p_pkt_id);
	if (ret != YAMC_RET_SUCCESS) return ret;

	return YAMC_RET_SUCCESS;
}

// encode and send packet
yamc_retcode_t yamc_send_pkt(yamc_instance_t* const p_instance, yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_ASSERT(p_instance != NULL);
	YAMC_ASSERT(p_pkt_data != NULL);

	switch (p_pkt_data->pkt_type)
	{
		case YAMC_PKT_CONNECT:
			return yamc_send_connect(p_instance, p_pkt_data);

		case YAMC_PKT_PUBLISH:
			return yamc_send_publish(p_instance, p_pkt_data);

		case YAMC_PKT_SUBSCRIBE:
			return yamc_send_subscribe(p_instance, p_pkt_data);

		//handle 'fixed header only' packets, fall through is intentional!
		case YAMC_PKT_PINGREQ:
		case YAMC_PKT_DISCONNECT:
			return yamc_send_fixed_hdr_only_pkt(p_instance, p_pkt_data);

		case YAMC_PKT_UNSUBSCRIBE:
			return yamc_send_unsubscribe(p_instance, p_pkt_data);

		case YAMC_PKT_PUBACK:
		case YAMC_PKT_PUBREL:
		case YAMC_PKT_PUBREC:
		case YAMC_PKT_PUBCOMP:
			return yamc_send_pub_x(p_instance, p_pkt_data);

		default:
			YAMC_ERROR_PRINTF("Unsupported packet type: %s\n", yamc_mqtt_pkt_type_to_str(p_pkt_data->pkt_type));
			return YAMC_RET_CANT_PARSE;
	}

	return YAMC_RET_SUCCESS;
}