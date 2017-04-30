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

///Socket write handler
typedef int (*yamc_write_handler_t)(uint8_t* p_buff, uint32_t buff_len);

///Disconnection request handler - signal main application that we should disconnect form server
typedef void (*yamc_disconnect_handler_t)(void);

///Start or pat (prolong) timeout timer
typedef void (*yamc_timeout_pat_handler_t)(void);

///Stop timeout timer
typedef void (*yamc_timeout_stop_handler_t)(void);

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

} yamc_handler_cfg_t;

///yamc instance struct
typedef struct
{
	yamc_mqtt_pkt_t 			rx_pkt;				///< Incoming packet buffer
	yamc_parser_state_t			parser_state;		///< Incoming packet parser state

	yamc_handler_cfg_t handlers;					///< event handlers

} yamc_instance_t;

//parse incoming data buffer
void yamc_parse_buff(yamc_instance_t* const p_instance, const uint8_t* const p_buff, uint32_t len);

///Initialize yamc instance
void yamc_init(yamc_instance_t* const p_instance, const yamc_handler_cfg_t* const p_handler_cfg);

#endif /* YAMC_H_ */
