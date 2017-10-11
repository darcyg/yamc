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
#include <stdbool.h>
#include "yamc_mqtt.h"

/// yamc return codes
typedef enum {
	YAMC_RET_SUCCESS = 0,	/// Success
	YAMC_RET_INVALID_DATA,   /// Data format error
	YAMC_RET_INVALID_STATE,  /// Invalid state
	YAMC_RET_CANT_PARSE,	 /// Parser error

} yamc_retcode_t;

/// yamc decoded MQTT packet data
typedef struct
{
	yamc_pkt_type_t pkt_type;  ///< packet type

	/// Packet flags
	struct
	{
		/**
		 * \brief Retain flag
		 *
		 * If the RETAIN flag is set to 1, in a PUBLISH Packet sent by a Client to a Server,
		 * the Server MUST store the Application Message and its QoS, so that it can be delivered
		 * to future subscribers whose subscriptions match its topic name
		*/
		uint8_t RETAIN : 1;

		/**
		 * This field indicates the level of assurance for delivery of an Application Message
		 */
		yamc_qos_lvl_t QOS : 2;

		/**
		 * \brief Duplicate delivery flag
		 *
		 * If the DUP flag is set to 0, it indicates that this is the first occasion that
		 * the Client or Server has attempted to send this MQTT PUBLISH Packet.
		 *
		 * If the DUP flag is set to 1, it indicates that this might be re-delivery of an
		 * earlier attempt to send the Packet
		 */
		uint8_t DUP : 1;

	} flags;

	/// Packet var_data
	union {
		yamc_mqtt_pkt_connect_t		connect;
		yamc_mqtt_pkt_connack_t		connack;
		yamc_mqtt_pkt_publish_t		publish;
		yamc_mqtt_pkt_puback_t		puback;
		yamc_mqtt_pkt_pubrec_t		pubrec;
		yamc_mqtt_pkt_pubrel_t		pubrel;
		yamc_mqtt_pkt_pubcomp_t		pubcomp;
		yamc_mqtt_pkt_subscribe_t   subscribe;
		yamc_mqtt_pkt_unsubscribe_t unsubscribe;
		yamc_mqtt_pkt_suback_t		suback;
		yamc_mqtt_pkt_unsuback_t	unsuback;

	} pkt_data;

} yamc_mqtt_pkt_data_t;

///Outgoing CONNECT packet definition
typedef struct
{
	bool			 clean_session;		   ///< Clean session flag
	yamc_qos_lvl_t   will_qos;			   ///< Will QoS value
	bool			 will_remain;		   ///< Will remain flag
	uint16_t		 keepalive_timeout_s;  ///< keepalive value in seconds
	yamc_mqtt_string client_id;			   ///< client identifier
	yamc_mqtt_string will_topic;		   ///< (optional) will topic
	yamc_mqtt_string will_message;		   ///< (optional) will message
	yamc_mqtt_string user_name;			   ///< (optional) user name
	yamc_mqtt_string password;			   ///< (optional) password

} yamc_connect_data_t;

///Outgoing PUBLISH packet definition
typedef struct
{
	yamc_mqtt_string topic;		///< publish topic
	yamc_qos_lvl_t   QOS;		///< QoS level
	bool			 DUP;		///< packet DUP flag
	bool			 RETAIN;	///< packet RETAIN flag
	const uint8_t*   p_data;	///< payload data
	uint32_t		 data_len;  ///< payload data length

} yamc_publish_data_t;

///Outgoing SUBSCRIBE packet definition
typedef yamc_mqtt_pkt_subscribe_topic_t yamc_subscribe_data_t;

/// yamc instance forward declaration
struct yamc_instance_s;

/// Socket write handler
typedef yamc_retcode_t (*yamc_write_handler_t)(void* p_ctx, const uint8_t* const p_buff, uint32_t buff_len);

/// Disconnection request handler - signal main application that we should disconnect form server
typedef void (*yamc_disconnect_handler_t)(void* p_ctx);

/// Start or pat (prolong) timeout timer
typedef void (*yamc_timeout_pat_handler_t)(void* p_ctx);

/// Stop timeout timer
typedef void (*yamc_timeout_stop_handler_t)(void* p_ctx);

/// New packet handler
typedef void (*yamc_pkt_handler_t)(struct yamc_instance_s* const p_instance, const yamc_mqtt_pkt_data_t* const p_pkt_data, void* p_ctx);

/// MQTT parser state enum
typedef enum {
	YAMC_PARSER_IDLE = 0,  ///< Idle state packet type and length unknown
	YAMC_PARSER_FIX_HDR,   ///< Parser is collecting fixed header data
	YAMC_PARSER_VAR_DATA,  ///< Parser is collecting variable header and/or data
	YAMC_PARSER_DONE,	  ///< Complete packet has been received
	YAMC_PARSER_SKIP_PKT   ///< Packet is too long to process, drop data until next one arrives

} yamc_parser_state_t;

typedef struct
{
	yamc_disconnect_handler_t   disconnect;		///< Server disconnection handler
	yamc_write_handler_t		write;			///< Write data to server handler
	yamc_timeout_pat_handler_t  timeout_pat;	///< start/restart timeout timer handler
	yamc_timeout_stop_handler_t timeout_stop;   ///< stop timeout timer handler
	yamc_pkt_handler_t			pkt_handler;	///< New packet handler
	void*						p_handler_ctx;  ///< handler context, can be null

} yamc_handler_cfg_t;

/// yamc instance struct
typedef struct yamc_instance_s
{
	yamc_handler_cfg_t  handlers;		 ///< event handlers
	yamc_mqtt_pkt_t		rx_pkt;			 ///< Incoming packet buffer
	yamc_parser_state_t parser_state;	///< Incoming packet parser state
	uint16_t			last_packet_id;  ///< id of last packet sent to server

	/// Enable parsing of given packet type
	struct
	{
		uint8_t CONNACK : 1;
		uint8_t PUBLISH : 1;
		uint8_t PUBACK : 1;
		uint8_t PUBREC : 1;
		uint8_t PUBREL : 1;
		uint8_t PUBCOMP : 1;
		uint8_t SUBACK : 1;
		uint8_t UNSUBACK : 1;
		uint8_t PINGRESP : 1;
	} parser_enables;

} yamc_instance_t;

/// Initialize yamc instance
void yamc_init(yamc_instance_t* const p_instance, const yamc_handler_cfg_t* const p_handler_cfg);

/// parse incoming data buffer
void yamc_parse_buff(yamc_instance_t* const p_instance, const uint8_t* const p_buff, uint32_t len);

///assign NULL terminated c string to yamc_mqtt_string object
void yamc_char_to_mqtt_str(const char* const p_char, yamc_mqtt_string* const p_str);

///Send CONNECT packet
yamc_retcode_t yamc_connect(const yamc_instance_t* const p_instance, const yamc_connect_data_t* const p_data);

///Set NULL terminated C string as PUBLISH message payload
void yamc_publish_set_char_payload(const char* const p_char, yamc_publish_data_t* const p_data);

///Send PUBLISH packet
yamc_retcode_t yamc_publish(yamc_instance_t* const p_instance, const yamc_publish_data_t* const p_data);

///Send SUBSCRIBE packet
yamc_retcode_t yamc_subscribe(yamc_instance_t* const p_instance, const yamc_subscribe_data_t* const p_data, uint16_t data_len);

//Send UNSUBSCRIBE packet
yamc_retcode_t yamc_unsubscribe(yamc_instance_t* const p_instance, const yamc_mqtt_string* const p_topics, uint16_t topics_len);

///Send PINGREQ packet
yamc_retcode_t yamc_ping(const yamc_instance_t* const p_instance);

///Send DISCONNECT packet
yamc_retcode_t yamc_disconnect(const yamc_instance_t* const p_instance);

///Send PUBACK packet
yamc_retcode_t yamc_puback(const yamc_instance_t* const p_instance, uint16_t packet_id);

///Send PUBREL packet
yamc_retcode_t yamc_pubrel(const yamc_instance_t* const p_instance, uint16_t packet_id);

///Send PUBREC packet
yamc_retcode_t yamc_pubrec(const yamc_instance_t* const p_instance, uint16_t packet_id);

///Send PUBCOMP packet
yamc_retcode_t yamc_pubcomp(const yamc_instance_t* const p_instance, uint16_t packet_id);

#endif /* YAMC_H_ */
