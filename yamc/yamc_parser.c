/*
 * YAMC - Yet Another MQTT Client library
 *
 * yamc_parser.c - Assembles incoming MQTT data into complete packet and calls yamc_pkt_decoder.
 *
 * Author: Michal Lower <https://github.com/keton>
 * 
 * Licensed under MIT License (see LICENSE file in main repo directory)
 *
 */

#include <stdbool.h>
#include <string.h>
#include "yamc.h"
#include "yamc_log.h"

/**
 * including what amounts to a .c file may be a bad taste
 * but allows to reduce stack pressure by statically inlininig yamc_decode_pkt()
 * and dependencies while keeping 2 separate files
 *
 * yamc_internal_pkt_decoder.h is wrapped inside of #ifdef __YAMC_INTERNALS__ to prevent accidental inclusion
 *
 */
#define __YAMC_INTERNALS__
#include <yamc_internal_pkt_decoder.h>
#undef __YAMC_INTERNALS__

// start or restart timeout timer
static inline void timeout_pat(const yamc_instance_t* const p_instance)
{
	YAMC_ASSERT(p_instance != NULL);

	if (p_instance->handlers.timeout_pat != NULL) p_instance->handlers.timeout_pat(p_instance->handlers.p_handler_ctx);
}

// stop timeout timer
static inline void timeout_stop(const yamc_instance_t* const p_instance)
{
	YAMC_ASSERT(p_instance != NULL);

	if (p_instance->handlers.timeout_stop != NULL) p_instance->handlers.timeout_stop(p_instance->handlers.p_handler_ctx);
}

/**
 * Store next 'remaining length' field byte and check if value can be decoded.
 * Packet length field can be 1-4 bytes long. This function returns false until result is properly decoded
 */
static inline uint8_t yamc_mqtt_decode_remaining_len(yamc_instance_t* const p_instance, const uint8_t data)
{
	YAMC_ASSERT(p_instance != NULL);

	yamc_mqtt_hdr_fixed_t* p_mqtt_hdr_fixed = &p_instance->rx_pkt.fixed_hdr;

	// store data and increment field length
	p_mqtt_hdr_fixed->remaining_len.raw[p_mqtt_hdr_fixed->remaining_len.raw_len++] = data;

	// return if field value is not complete
	if ((data & 0x80) != 0) return false;

	// decode field value - algorithm taken from MQTT specification

	uint32_t multiplier  = 1;
	uint32_t value		 = 0;
	uint8_t  field_idx   = 0;
	uint8_t  encodedByte = 0;
	do
	{
		encodedByte = p_mqtt_hdr_fixed->remaining_len.raw[field_idx++];
		value += (encodedByte & 127) * multiplier;
		if (multiplier > 128 * 128 * 128)
		{
			YAMC_LOG_ERROR("Malformed Remaining Length\n");
			p_instance->handlers.disconnect(p_instance->handlers.p_handler_ctx);
			return false;
		}
		multiplier *= 128;

	} while ((encodedByte & 128) != 0);

	p_mqtt_hdr_fixed->remaining_len.decoded_val = value;
	return true;
}

/// Initialize yamc instance
void yamc_init(yamc_instance_t* const p_instance, const yamc_handler_cfg_t* const p_handler_cfg)
{
	YAMC_ASSERT(p_instance != NULL);
	YAMC_ASSERT(p_handler_cfg != NULL);

	// null check of handlers
	YAMC_ASSERT(p_handler_cfg->pkt_handler != NULL);
	YAMC_ASSERT(p_handler_cfg->disconnect != NULL);
	YAMC_ASSERT(p_handler_cfg->write != NULL);

	// timeout handlers are optional and null checked at execution

	memset(p_instance, 0, sizeof(yamc_instance_t));
	memcpy(&p_instance->handlers, p_handler_cfg, sizeof(yamc_handler_cfg_t));
}

// parse incoming data buffer
void yamc_parse_buff(yamc_instance_t* const p_instance, const uint8_t* const p_buff, uint32_t len)
{
	YAMC_ASSERT(p_instance != NULL);
	YAMC_ASSERT(p_buff != NULL);

	YAMC_LOG_DEBUG("Raw data:");
	yamc_log_hex(p_buff, len);

	// there's data for more than one parser state, repeat
	uint8_t reparse = false;

	// there's data for more than one packet, restart after YAMC_PARSER_DONE
	uint8_t next_packet_present = false;

	// current position in p_buff data
	uint32_t buff_pos = 0;

	// values used by YAMC_PARSER_VAR_DATA memcpy() calls
	uint32_t	   bytes_to_copy = 0;
	const uint8_t* p_var_data_start;

	// start or reset timeout measurement
	timeout_pat(p_instance);

	// packet assembly state machine
	do  // while (reparse)
	{
		reparse							  = false;
		uint8_t decode_remaining_len_done = false;

		switch (p_instance->parser_state)
		{
			// Capture packet type and go to YAMC_PARSER_FIX_HDR state
			case YAMC_PARSER_IDLE:
				YAMC_LOG_DEBUG("State: YAMC_PARSER_IDLE\n");

				memset(&p_instance->rx_pkt, 0, sizeof(yamc_mqtt_pkt_t));

				// store packet type
				p_instance->rx_pkt.fixed_hdr.pkt_type.raw = p_buff[buff_pos];

				// error-check packet type
				if (p_instance->rx_pkt.fixed_hdr.pkt_type.flags.type > YAMC_PKT_DISCONNECT ||
					p_instance->rx_pkt.fixed_hdr.pkt_type.flags.type < YAMC_PKT_CONNECT)
				{
					YAMC_LOG_ERROR("Invalid packet type: %02X\n", p_instance->rx_pkt.fixed_hdr.pkt_type.flags.type);
					p_instance->handlers.disconnect(p_instance->handlers.p_handler_ctx);
					return;
				}

				// go to YAMC_PARSER_FIX_HDR state
				p_instance->parser_state = YAMC_PARSER_FIX_HDR;
				buff_pos++;

				// if there's more data in p_buff continue parsing
				if (buff_pos < len) reparse = true;

				break;

			// Parser is collecting fixed header data
			case YAMC_PARSER_FIX_HDR:
				YAMC_LOG_DEBUG("State: YAMC_PARSER_FIX_HDR\n");

				decode_remaining_len_done = false;

				// decode 'remaining length' field in fixed header
				while ((decode_remaining_len_done = yamc_mqtt_decode_remaining_len(p_instance, p_buff[buff_pos++])) == false &&
					   buff_pos < len)
					;

				//'remaining length' field is not yet fully decoded, wait for more data
				if (decode_remaining_len_done == false) return;

				// error check decoded length
				if (p_instance->rx_pkt.fixed_hdr.remaining_len.decoded_val > YAMC_MQTT_MAX_LEN)
				{
					YAMC_LOG_ERROR("Decoded var_data length exceeds MQTT spec.\n");
					p_instance->handlers.disconnect(p_instance->handlers.p_handler_ctx);
					return;
				}

				// go to YAMC_PARSER_VAR_DATA or YAMC_PARSER_SKIP_PKT based on if we can fit rest of the packet into
				// rx_buffer
				if (p_instance->rx_pkt.fixed_hdr.remaining_len.decoded_val < YAMC_RX_PKT_MAX_LEN)
					p_instance->parser_state = YAMC_PARSER_VAR_DATA;
				else
					p_instance->parser_state = YAMC_PARSER_SKIP_PKT;

				// if there's more data or packet doesn't contain var_data field immediately go to next state via reparse
				// flag
				if (buff_pos < len || p_instance->rx_pkt.fixed_hdr.remaining_len.decoded_val == 0) reparse = true;
				break;

			case YAMC_PARSER_SKIP_PKT:  ///< Packet is too long to process, drop data until next one arrives
				YAMC_LOG_DEBUG("State: YAMC_PARSER_SKIP_PKT\n");

			// intentional fall through!

			case YAMC_PARSER_VAR_DATA:  ///< Parser is collecting variable header and payload data

				YAMC_LOG_DEBUG("State: YAMC_PARSER_VAR_DATA\n");

				bytes_to_copy	= len - buff_pos;
				p_var_data_start = &p_buff[buff_pos];

				// LOG_DEBUG("buff_pos: %d, bytes_to_copy:%d\n",buff_pos,bytes_to_copy);

				// check if this packet is without var_data
				if (p_instance->rx_pkt.fixed_hdr.remaining_len.decoded_val == 0)
				{
					// there's more than one packet in buffer
					if (bytes_to_copy > 0)
					{
						YAMC_LOG_DEBUG("no var_data: More than one packet\n");

						// since there's more data in p_buff more than one packet is present
						next_packet_present = true;
					}
				}
				// check if all incoming data fits completely into var_data of this packet
				else if (p_instance->rx_pkt.var_data.pos + bytes_to_copy <= p_instance->rx_pkt.fixed_hdr.remaining_len.decoded_val)
				{
					// don't copy data in skip packet state
					if (p_instance->parser_state != YAMC_PARSER_SKIP_PKT)
						memcpy(&p_instance->rx_pkt.var_data.data[p_instance->rx_pkt.var_data.pos], p_var_data_start, bytes_to_copy);

					// but always save receive progress
					p_instance->rx_pkt.var_data.pos += bytes_to_copy;

					// no need to update buff_pos since we've consumed whole p_buff
				}
				// if not copy only var_data bytes
				else
				{
					YAMC_LOG_DEBUG("More than one packet present\n");

					uint32_t buffer_remaining_len =
						p_instance->rx_pkt.fixed_hdr.remaining_len.decoded_val - p_instance->rx_pkt.var_data.pos;

					if (buffer_remaining_len > bytes_to_copy)
					{
						YAMC_LOG_ERROR("Too much to copy!\n");
						buffer_remaining_len = bytes_to_copy;
					}

					// don't copy data in skip packet state
					if (p_instance->parser_state != YAMC_PARSER_SKIP_PKT)
						memcpy(&p_instance->rx_pkt.var_data.data[p_instance->rx_pkt.var_data.pos], p_var_data_start, buffer_remaining_len);

					// but always save receive progress

					// update buff_pos since there's more data to process in p_buff
					buff_pos += buffer_remaining_len;
					p_instance->rx_pkt.var_data.pos += buffer_remaining_len;

					// since there's more data in p_buff more than one packet is present
					next_packet_present = true;
				}

				// all current packet's var_data have been processed
				if (p_instance->rx_pkt.var_data.pos == p_instance->rx_pkt.fixed_hdr.remaining_len.decoded_val)
				{
					// var_data were stored, go to YAMC_PARSER_DONE
					if (p_instance->parser_state != YAMC_PARSER_SKIP_PKT)
					{
						// LOG_DEBUG("packet rx complete\n");
						p_instance->parser_state = YAMC_PARSER_DONE;
						reparse					 = true;
					}
					else  // var_data were skipped, go to YAMC_PARSER_IDLE
					{
						p_instance->parser_state = YAMC_PARSER_IDLE;
						// reparse only if there's next packet present
						if (next_packet_present)
						{
							next_packet_present = false;
							reparse				= true;
		
							// rearm timeout timer
							timeout_pat(p_instance);
						}
					}
				}
				break;

			case YAMC_PARSER_DONE:  ///< Complete packet has been received
				YAMC_LOG_DEBUG("State: YAMC_PARSER_DONE\n");

				// stop timeout measurement
				timeout_stop(p_instance);

				// pass execution to packet data decoders, this will launch 'new packet arrived' handler
				yamc_decode_pkt(p_instance);

				// go to idle state and wait for next packet
				p_instance->parser_state = YAMC_PARSER_IDLE;

				// if there's more data in p_buff reparse immediately
				if (next_packet_present)
				{
					next_packet_present = false;
					reparse				= true;

					// rearm timeout timer
					timeout_pat(p_instance);
				}

				break;

		}  // end of switch (p_instance->parser_state)

	} while (reparse);
}
