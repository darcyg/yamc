/*
 * YAMC - Yet Another MQTT Client library
 *
 * yamc_pkt_decoder.c - Parses MQTT packet data and calls user defined event handler.
 *
 * Author: Michal Lower <keton22@gmail.com>
 *
 * All rights reserved 2017
 *
 */

/*
 * this file is intended to be included as a part of yamc_parser.c
 *
 * Wrapping in #ifdef __YAMC_INTERNALS__ to prevent accidental inclusion
 *
 */
#ifdef __YAMC_INTERNALS__

#ifndef __YAMC_INTERNAL_PKT_DECODER_H__
#define __YAMC_INTERNAL_PKT_DECODER_H__

#include "yamc.h"
#include "yamc_log.h"

/// returns true if user enabled parsing of given packet type
static inline uint8_t is_parsing_enabled(const yamc_instance_t* const p_instance, yamc_pkt_type_t pkt_type)
{
	YAMC_ASSERT(p_instance != NULL);

	switch (pkt_type)
	{
		case YAMC_PKT_CONNACK:
			return p_instance->parser_enables.CONNACK;

		case YAMC_PKT_PUBLISH:
			return p_instance->parser_enables.PUBLISH;

		case YAMC_PKT_PUBACK:
			return p_instance->parser_enables.PUBACK;

		case YAMC_PKT_PUBREC:
			return p_instance->parser_enables.PUBREC;

		case YAMC_PKT_PUBREL:
			return p_instance->parser_enables.PUBREL;

		case YAMC_PKT_PUBCOMP:
			return p_instance->parser_enables.PUBCOMP;

		case YAMC_PKT_SUBACK:
			return p_instance->parser_enables.SUBACK;

		case YAMC_PKT_UNSUBACK:
			return p_instance->parser_enables.UNSUBACK;

		case YAMC_PKT_PINGRESP:
			return p_instance->parser_enables.PINGRESP;

		default:
			YAMC_LOG_DEBUG("Unknown packet type %d\n", pkt_type);
			return false;
	}

	return false;
}

/// Copy packet type and flags to yamc_mqtt_pkt_data_t struct
static inline void fill_pkt_header(const yamc_instance_t* const p_instance, yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_ASSERT(p_instance != NULL);
	YAMC_ASSERT(p_pkt_data != NULL);

	// pkt type
	p_pkt_data->pkt_type = p_instance->rx_pkt.fixed_hdr.pkt_type.flags.type;

	// pkt flags
	p_pkt_data->flags.DUP	= p_instance->rx_pkt.fixed_hdr.pkt_type.flags.DUP;
	p_pkt_data->flags.QOS	= p_instance->rx_pkt.fixed_hdr.pkt_type.flags.QOS;
	p_pkt_data->flags.RETAIN = p_instance->rx_pkt.fixed_hdr.pkt_type.flags.RETAIN;
}

// decode 16 bit number, MQTT uses unsigned 16 bit ints in Big Endian
static inline uint16_t decode_mqtt_word(const uint8_t* const p_word)
{
	YAMC_ASSERT(p_word != NULL);

	return (uint16_t)p_word[0] << 8 | p_word[1];
}

/**
 * \brief decode MQTT string into yamc_mqtt_string
 *
 * \return YAMC_SUCCESS or YAMC_ERROR_INVALID_DATA when decoded string length exceeds remaining var_data length
 * \param[in/out] len in: remaining var_data length, out: by how many bytes to advance parse buffer position
 */
static yamc_retcode_t decode_mqtt_string(uint8_t* const p_raw_data, uint32_t* p_len, yamc_mqtt_string* const p_mqtt_str)
{
	YAMC_ASSERT(p_raw_data != NULL);
	YAMC_ASSERT(p_mqtt_str != NULL);
	YAMC_ASSERT(p_len != NULL);

	// minimal MQTT string is 3 bytes long - 2 bytes of length + 1 char
	if ((*p_len) < 3) return YAMC_RET_INVALID_DATA;

	uint16_t str_len = decode_mqtt_word(p_raw_data);

	// decoded string length + string length field is longer that remaining data
	if (str_len + 2u > (*p_len)) return YAMC_RET_INVALID_DATA;

	// store pointer to string
	p_mqtt_str->str = &p_raw_data[2];

	// store string length
	p_mqtt_str->len = str_len;

	// store buffer position increment - string length + 2 bytes for length data
	*p_len = str_len + 2;

	return YAMC_RET_SUCCESS;
}

static inline yamc_retcode_t yamc_decode_connack(const yamc_instance_t* const p_instance, yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_ASSERT(p_instance != NULL);
	YAMC_ASSERT(p_pkt_data != NULL);

	if (p_instance->rx_pkt.fixed_hdr.remaining_len.decoded_val != 2)
	{
		YAMC_LOG_ERROR("Wrong packet len: %d or %d\n", p_instance->rx_pkt.var_data.pos,
					   p_instance->rx_pkt.fixed_hdr.remaining_len.decoded_val);
		return YAMC_RET_CANT_PARSE;
	}

	yamc_mqtt_pkt_connack_t* const p_dest_pkt = &p_pkt_data->pkt_data.connack;
	const uint8_t* const		   p_raw_data = p_instance->rx_pkt.var_data.data;

	p_dest_pkt->ack_flags.raw = p_raw_data[0];
	p_dest_pkt->return_code   = (yamc_mqtt_connack_retcode_t)p_raw_data[1];

	return YAMC_RET_SUCCESS;
}

static inline yamc_retcode_t yamc_decode_publish(yamc_instance_t* const p_instance, yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_ASSERT(p_instance != NULL);
	YAMC_ASSERT(p_pkt_data != NULL);

	yamc_mqtt_pkt_publish_t* const p_dest_pkt = &p_pkt_data->pkt_data.publish;
	uint8_t* const				   p_raw_data = p_instance->rx_pkt.var_data.data;

	uint32_t raw_data_pos = 0;
	uint32_t pkt_length   = p_instance->rx_pkt.fixed_hdr.remaining_len.decoded_val;
	uint32_t rem_length   = pkt_length;

	yamc_retcode_t ret_code;

	ret_code = decode_mqtt_string(p_raw_data, &rem_length, &p_dest_pkt->topic_name);
	if (ret_code != YAMC_RET_SUCCESS) return YAMC_RET_CANT_PARSE;

	raw_data_pos += rem_length;
	rem_length = pkt_length - raw_data_pos;

	// on QoS greater than zero there's 2 byte packet id field
	if (p_pkt_data->flags.QOS > 0)
	{
		p_dest_pkt->packet_id = decode_mqtt_word(&p_raw_data[raw_data_pos]);
		raw_data_pos += 2;

		if (raw_data_pos > pkt_length) return YAMC_RET_CANT_PARSE;

		rem_length = pkt_length - raw_data_pos;
	}

	// sanity check of payload length
	if (rem_length > pkt_length) return YAMC_RET_CANT_PARSE;

	// rest is topic payload
	p_dest_pkt->payload.data_len				   = rem_length;
	if (rem_length > 0) p_dest_pkt->payload.p_data = &p_raw_data[raw_data_pos];  // leave null pointer if payload is zero bytes long

	return YAMC_RET_SUCCESS;
}

static inline yamc_retcode_t yamc_decode_pub_x(const yamc_instance_t* const p_instance, yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_ASSERT(p_instance != NULL);
	YAMC_ASSERT(p_pkt_data != NULL);

	if (p_instance->rx_pkt.fixed_hdr.remaining_len.decoded_val != 2)
	{
		YAMC_LOG_ERROR("Wrong packet len: %d or %d\n", p_instance->rx_pkt.var_data.pos,
					   p_instance->rx_pkt.fixed_hdr.remaining_len.decoded_val);
		return YAMC_RET_CANT_PARSE;
	}

	const uint8_t* const		  p_raw_data = p_instance->rx_pkt.var_data.data;
	yamc_mqtt_pkt_generic_pubx_t* p_dest_pkt;

	switch (p_pkt_data->pkt_type)
	{
		case YAMC_PKT_PUBACK:
			p_dest_pkt = &p_pkt_data->pkt_data.puback;
			break;

		case YAMC_PKT_PUBREC:
			p_dest_pkt = &p_pkt_data->pkt_data.pubrec;
			break;

		case YAMC_PKT_PUBREL:
			p_dest_pkt = &p_pkt_data->pkt_data.pubrel;
			break;

		case YAMC_PKT_PUBCOMP:
			p_dest_pkt = &p_pkt_data->pkt_data.pubcomp;
			break;

		case YAMC_PKT_UNSUBACK:
			p_dest_pkt = &p_pkt_data->pkt_data.unsuback;
			break;

		default:
			return YAMC_RET_CANT_PARSE;
	}

	p_dest_pkt->packet_id = decode_mqtt_word(&p_raw_data[0]);

	return YAMC_RET_SUCCESS;
}

static inline yamc_retcode_t yamc_decode_suback(yamc_instance_t* const p_instance, yamc_mqtt_pkt_data_t* const p_pkt_data)
{
	YAMC_ASSERT(p_instance != NULL);
	YAMC_ASSERT(p_pkt_data != NULL);

	yamc_mqtt_pkt_suback_t* const p_dest_pkt = &p_pkt_data->pkt_data.suback;
	uint8_t* const				  p_raw_data = p_instance->rx_pkt.var_data.data;
	uint32_t					  pkt_length = p_instance->rx_pkt.fixed_hdr.remaining_len.decoded_val;

	// minimal suback packet is 3 bytes long
	if (pkt_length < 3) return YAMC_RET_CANT_PARSE;

	// first 2 bytes - packet id
	p_dest_pkt->pkt_id = decode_mqtt_word(p_raw_data);

	// rest - array of return codes
	p_dest_pkt->payload.p_retcodes   = &p_raw_data[2];
	p_dest_pkt->payload.retcodes_len = pkt_length - 2;

	return YAMC_RET_SUCCESS;
}

// decode assembled MQTT packet data and call user defined event handler
static inline void yamc_decode_pkt(yamc_instance_t* const p_instance)
{
	YAMC_ASSERT(p_instance != NULL);

	// log raw packet data, TODO: remove logging after we're done
	yamc_log_raw_pkt(p_instance);

	// terminate if parsing of given packet type is not enabled
	if (!is_parsing_enabled(p_instance, p_instance->rx_pkt.fixed_hdr.pkt_type.flags.type)) return;

	yamc_retcode_t		 decoder_retcode = YAMC_RET_CANT_PARSE;
	yamc_mqtt_pkt_data_t mqtt_pkt_data;

	memset(&mqtt_pkt_data, 0, sizeof(yamc_mqtt_pkt_data_t));

	/// fill in packet type and flags using instance data
	fill_pkt_header(p_instance, &mqtt_pkt_data);

	// decode data according to packet type
	switch (p_instance->rx_pkt.fixed_hdr.pkt_type.flags.type)
	{
		case YAMC_PKT_CONNACK:
			decoder_retcode = yamc_decode_connack(p_instance, &mqtt_pkt_data);
			break;

		case YAMC_PKT_PUBLISH:
			decoder_retcode = yamc_decode_publish(p_instance, &mqtt_pkt_data);
			break;

		case YAMC_PKT_PUBACK:
		case YAMC_PKT_PUBREC:
		case YAMC_PKT_PUBREL:
		case YAMC_PKT_PUBCOMP:
		case YAMC_PKT_UNSUBACK:
			decoder_retcode = yamc_decode_pub_x(p_instance, &mqtt_pkt_data);
			break;

		case YAMC_PKT_SUBACK:
			decoder_retcode = yamc_decode_suback(p_instance, &mqtt_pkt_data);
			break;

		case YAMC_PKT_PINGRESP:
			// pingresp has no var_data, nothing to parse
			decoder_retcode = YAMC_RET_SUCCESS;
			break;

		default:
			YAMC_LOG_ERROR("Unknown packet type %d\n", p_instance->rx_pkt.fixed_hdr.pkt_type.flags.type);
			break;
	}

	// if packet was decoded successfully launch user handler
	if (decoder_retcode == YAMC_RET_SUCCESS) p_instance->handlers.pkt_handler(p_instance, &mqtt_pkt_data, p_instance->handlers.p_handler_ctx);
}

#endif /* ifdef __YAMC_INTERNAL_PKT_DECODER_H__ */

#endif /* ifdef __YAMC_INTERNALS__ */
