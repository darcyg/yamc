/*
 * YAMC - Yet Another MQTT Client library
 *
 * yamc_mqtt.h - MQTT protocol data structures and definitions
 *
 * Author: Michal Lower <keton22@gmail.com>
 *
 * All rights reserved 2017
 *
 */

#ifndef __YAMC_MQTT_H__
#define __YAMC_MQTT_H__

#include <stdint.h>

///Payload buffer length, any payload longer than this will not be processed
#define YAMC_PKT_MAX_LEN 1024

///Maximum MQTT message length allowed by standard
#define YAMC_MQTT_MAX_LEN 268435455

///MQTT packet type
typedef enum
{
	YAMC_PKT_CONNECT=1,
	YAMC_PKT_CONNACK,
	YAMC_PKT_PUBLISH,
	YAMC_PKT_PUBACK,
	YAMC_PKT_PUBREC,
	YAMC_PKT_PUBREL,
	YAMC_PKT_PUBCOMP,
	YAMC_PKT_SUBSCRIBE,
	YAMC_PKT_SUBACK,
	YAMC_PKT_UNSUBSCRIBE,
	YAMC_PKT_UNSUBACK,
	YAMC_PKT_PINGREQ,
	YAMC_PKT_PINGRESP,
	YAMC_PKT_DISCONNECT,
} yamc_pkt_type_t;

///MQTT QoS levels
typedef enum
{
	YAMC_QOS_LVL0=0,	///< At most once delivery
	YAMC_QOS_LVL1,		///< At least once delivery
	YAMC_QOS_LVL2		///< Exactly once delivery

} yamc_qos_lvl_t;

///Maximum width in bytes of 'remaining length' field
#define YAMC_MQTT_REM_LEN_MAX 4


//MQTT string
typedef struct
{
	uint8_t* str; //variable length UTF-8 string. MQTT strings are NOT \0 terminated!
	uint16_t len; //string length
} yamc_mqtt_string;

///Fixed header
typedef struct
{
	///MQTT message type and flags
	union
	{
		struct
		{
			/**
			 * \brief Retain flag
			 *
			 * If the RETAIN flag is set to 1, in a PUBLISH Packet sent by a Client to a Server,
			 * the Server MUST store the Application Message and its QoS, so that it can be delivered
			 * to future subscribers whose subscriptions match its topic name
			*/
			const uint8_t 			RETAIN:1;

			/**
			 * This field indicates the level of assurance for delivery of an Application Message
			 */
			const yamc_qos_lvl_t 	QOS:2;

			/**
			 * \brief Duplicate delivery flag
			 *
			 * If the DUP flag is set to 0, it indicates that this is the first occasion that
			 * the Client or Server has attempted to send this MQTT PUBLISH Packet.
			 *
			 * If the DUP flag is set to 1, it indicates that this might be re-delivery of an
			 * earlier attempt to send the Packet
			 */
			const uint8_t 			DUP:1;


			const yamc_pkt_type_t 	type:4;		///< MQTT packet type

		} flags;

		uint8_t raw;

	} pkt_type;

	/**
	 * The Remaining Length is the number of bytes remaining within the current packet,
	 * including data in the variable header and the payload.
	 * The Remaining Length does not include the bytes used to encode the Remaining Length.
	*/
	struct
	{
		uint8_t 	raw[YAMC_MQTT_REM_LEN_MAX];		///< Buffer for 'remaining length' raw data
		uint8_t 	raw_len;						///< how many bytes are stored in raw
		uint32_t 	decoded_val;					///< decoded remaining length value

	} remaining_len;


} yamc_mqtt_hdr_fixed_t;

//CONNECT packet variable data definition
typedef struct
{
	//variable header

	yamc_mqtt_string 	protocol_name;	///MQTT string representing protocol name "MQTT"
	uint8_t 			protocol_lvl;	///MQTT protocol level: 4 for MQTT 3.1.1

	///Connect flags
	union
	{
		struct
		{
			const uint8_t 	res:1; 				///< reserved
			uint8_t 		clean_session:1;	///< clean sesion flag
			uint8_t 		will_flag:1;		///< Will present flag
			uint8_t 		will_qos:2;			///< Will QoS
			uint8_t 		will_remain:1;		///< Will remain
			uint8_t 		password_flag:1;	///< password present flag
			uint8_t 		username_flag:1;	///< user name present flag

		} flags;

		uint8_t raw;

	} connect_flags;

	uint16_t keepalive_timeout_s;	///< keepalive value in seconds

	//payload
	yamc_mqtt_string client_id;		///< client identifier
	yamc_mqtt_string will_topic;	///< (optional) will topic
	yamc_mqtt_string will_message;	///< (optional) will message
	yamc_mqtt_string user_name;		///< (optional) user name
	yamc_mqtt_string password;		///< (optional) password


} yamc_mqtt_pkt_connect_t;

///Connack return codes
typedef enum
{
	YAMC_CONNACK_ACCEPTED=0,		///< Connection accepted
	YAMC_CONNACK_REFUSED_VERSION,	///< The Server does not support the level of the MQTT protocol requested by the Client
	YAMC_CONNACK_REFUSED_ID,		///< The Client identifier is correct UTF-8 but not allowed by the Server
	YAMC_CONNACK_REFUSED_UNAVAIL,	///< The Network Connection has been made but the MQTT service is unavailable
	YAMC_CONNACK_REFUSED_USER_PASS,	///< The data in the user name or password is malformed
	YAMC_CONNACK_REFUSED_AUTH		///< The Client is not authorized to connect

} yamc_mqtt_connack_retcode_t;

///MQTT CONNACK packet variable data definition
typedef struct
{
	//variable header
	union
	{
		struct
		{
			uint8_t 		session_present:1; 	///< session present
			const uint8_t 	res:7;				///< reserved

		} flags;

		uint8_t raw;

	} ack_flags;

	yamc_mqtt_connack_retcode_t return_code;	///< return code

	//no payload

} yamc_mqtt_pkt_connack_t;

///MQTT PUBLISH packet var_data definition
typedef struct
{
	//variable header

	///Topic name in MQTT string format
	yamc_mqtt_string 	topic_name;

	/**
	 * \brief Unique packet id.
	 *
	 * Retransmissions of the same message must have the same packet id. IDs are released on PUBACK.
	 *
	 * The Packet Identifier field is only present in PUBLISH Packets where the QoS level is 1 or 2.
	 *
	 */
	uint16_t			packet_id;

	///payload
	struct
	{
		/**
		 * \brief payload data
		 *
		 * Payload contains the Application Message that is being published.
		 * The content and format of the data is application specific
		 */
		uint8_t* p_data;

		/**
		 * \brief Payload length
		 *
		 * The length of the payload can be calculated by subtracting the length
		 * of the variable header from the Remaining Length field that is in the Fixed Header.
		 *
		 * It is valid for a PUBLISH Packet to contain a zero length payload.
		 */
		uint32_t data_len;
	} payload;

} yamc_mqtt_pkt_publish_t;

///MQTT PUBACK, PUBREC, PUBREL, PUBCOMP var_data definition
typedef struct
{
	/**
	 * \brief Unique packet id.
	 *
	 * Retransmissions of the same message must have the same packet id. IDs are released on PUBACK.
	 *
	 */
	uint16_t			packet_id;

} yamc_mqtt_pkt_generic_pubx_t;

typedef yamc_mqtt_pkt_generic_pubx_t yamc_mqtt_pkt_puback_t;
typedef yamc_mqtt_pkt_generic_pubx_t yamc_mqtt_pkt_pubrec_t;
typedef yamc_mqtt_pkt_generic_pubx_t yamc_mqtt_pkt_pubrel_t;
typedef yamc_mqtt_pkt_generic_pubx_t yamc_mqtt_pkt_pubcomp_t;

///MQTT SUBSCRIBE topic filter/ QoS pair
typedef struct
{
	yamc_mqtt_string topic_name; 	///< Topic name filter

	///QoS definition
	struct
	{
		yamc_qos_lvl_t 	lvl:2; 		///< QoS level
		const uint8_t 	res:6;		///< reserved

	} qos;

} yamc_mqtt_pkt_subscribe_topic_t;

///MQTT SUBSCRIBE packet var_data
typedef struct
{
	//variable header

	uint16_t pkt_id; 	///< packet identifier

	//payload
	struct
	{
		yamc_mqtt_pkt_subscribe_topic_t* p_topics;	///< array of topic filter definitions
		uint16_t topics_len;						///< length of topics array

	} payload;


} yamc_mqtt_pkt_subscribe_t;

///SUBACK return code values
typedef enum
{
	YAMC_SUBACK_SUCC_QOS0=0,	///< Success - Maximum QoS 0
	YAMC_SUBACK_SUCC_QOS1,		///< Success - Maximum QoS 1
	YAMC_SUBACK_SUCC_QOS2,		///< Success - Maximum QoS 2
	YAMC_SUBACK_FAIL=0x80		///< Failure

} yamc_suback_retcode_t;

///MQTT SUBACK packet var_data
typedef struct
{
	//variable header

	uint16_t pkt_id; 	///< packet identifier

	//payload
	struct
	{
		uint8_t * 	p_retcodes;		///< array of return codes
		uint16_t	retcodes_len;	///< length of return codes array

	} payload;

} yamc_mqtt_pkt_suback_t;

///MQTT packet struct
typedef struct
{
	yamc_mqtt_hdr_fixed_t 	fixed_hdr;			///< For fixed packet header

	///var_data - buffer for variable header and payload packet fields
	struct
	{
		uint8_t 	data[YAMC_PKT_MAX_LEN+1];	///< Raw packet data buffer except fixed header
		uint32_t 	pos;						///< raw data write pointer position

	} var_data;

} yamc_mqtt_pkt_t;

#endif /* __YAMC_MQTT_H__ */
