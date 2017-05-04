/*
 * YAMC - Yet Another MQTT Client library
 *
 * yamc.h - Main include file
 *
 * Author: Michal Lower <keton22@gmail.com>
 *
 * All rights reserved 2017
 *
 */

#ifndef __YAMC_H__
#define __YAMC_H__

#include <stdint.h>
#include "yamc_mqtt.h"

///yamc return codes
typedef enum
{
	YAMC_SUCCESS=0,
	YAMC_ERROR_INVALID_DATA,
	YAMC_ERROR_INVALID_STATE,
	YAMC_ERROR_NULL,
	YAMC_ERROR_CANT_PARSE,

} yamc_retcode_t;

///yamc decoded MQTT packet data
typedef struct
{
	yamc_pkt_type_t pkt_type;					///< packet type

	///Packet flags
	struct
	{
		/**
		 * \brief Retain flag
		 *
		 * If the RETAIN flag is set to 1, in a PUBLISH Packet sent by a Client to a Server,
		 * the Server MUST store the Application Message and its QoS, so that it can be delivered
		 * to future subscribers whose subscriptions match its topic name
		*/
		uint8_t 			RETAIN:1;

		/**
		 * This field indicates the level of assurance for delivery of an Application Message
		 */
		yamc_qos_lvl_t 	QOS:2;

		/**
		 * \brief Duplicate delivery flag
		 *
		 * If the DUP flag is set to 0, it indicates that this is the first occasion that
		 * the Client or Server has attempted to send this MQTT PUBLISH Packet.
		 *
		 * If the DUP flag is set to 1, it indicates that this might be re-delivery of an
		 * earlier attempt to send the Packet
		 */
		uint8_t 			DUP:1;

	} flags;

	///Packet var_data
	union
	{
		yamc_mqtt_pkt_connect_t 	connect;
		yamc_mqtt_pkt_connack_t		connack;
		yamc_mqtt_pkt_publish_t		publish;
		yamc_mqtt_pkt_puback_t		puback;
		yamc_mqtt_pkt_pubrec_t		pubrec;
		yamc_mqtt_pkt_pubrel_t		pubrel;
		yamc_mqtt_pkt_pubcomp_t		pubcomp;
		yamc_mqtt_pkt_subscribe_t	subscribe;
		yamc_mqtt_pkt_suback_t		suback;
		yamc_mqtt_pkt_unsuback_t	unsuback;

	} pkt_data;

} yamc_mqtt_pkt_data_t;

///yamc instance forward declaration
struct yamc_instance_s;

///Socket write handler
typedef int (*yamc_write_handler_t)(uint8_t* p_buff, uint32_t buff_len);

///Disconnection request handler - signal main application that we should disconnect form server
typedef void (*yamc_disconnect_handler_t)(void);

///Start or pat (prolong) timeout timer
typedef void (*yamc_timeout_pat_handler_t)(void);

///Stop timeout timer
typedef void (*yamc_timeout_stop_handler_t)(void);

///New packet handler
typedef void (*yamc_pkt_handler_t)(struct yamc_instance_s* const p_instance, const yamc_mqtt_pkt_data_t* const p_pkt_data);

///MQTT parser state enum
typedef enum
{
	YAMC_PARSER_IDLE = 0,		///< Idle state packet type and length unknown
	YAMC_PARSER_FIX_HDR,		///< Parser is collecting fixed header data
	YAMC_PARSER_VAR_DATA,		///< Parser is collecting variable header and/or data
	YAMC_PARSER_DONE,			///< Complete packet has been received
	YAMC_PARSER_SKIP_PKT		///< Packet is too long to process, drop data until next one arrives

} yamc_parser_state_t;

typedef struct
{
	yamc_disconnect_handler_t 	disconnect;		///< Server disconnection handler
	yamc_write_handler_t 		write;			///< Write data to server handler
	yamc_timeout_pat_handler_t	timeout_pat;	///< start/restart timeout timer handler
	yamc_timeout_stop_handler_t timeout_stop;	///< stop timeout timer handler
	yamc_pkt_handler_t			pkt_handler;	///< New packet handler

} yamc_handler_cfg_t;

///yamc instance struct
typedef struct yamc_instance_s
{
	yamc_handler_cfg_t 			handlers;			///< event handlers
	yamc_mqtt_pkt_t 			rx_pkt;				///< Incoming packet buffer
	yamc_parser_state_t			parser_state;		///< Incoming packet parser state

	///Enable parsing of given packet type
	struct
	{
		uint8_t CONNACK:1;
		uint8_t PUBLISH:1;
		uint8_t PUBACK:1;
		uint8_t PUBREC:1;
		uint8_t PUBREL:1;
		uint8_t PUBCOMP:1;
		uint8_t SUBACK:1;
		uint8_t UNSUBACK:1;
		uint8_t PINGRESP:1;
	} parser_enables;

} yamc_instance_t;

//parse incoming data buffer
void yamc_parse_buff(yamc_instance_t* const p_instance, const uint8_t* const p_buff, uint32_t len);

///Initialize yamc instance
void yamc_init(yamc_instance_t* const p_instance, const yamc_handler_cfg_t* const p_handler_cfg);

#endif /* YAMC_H_ */
