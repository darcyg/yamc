/*
 * YAMC - Yet Another MQTT Client library
 *
 * yamc_port.h - Platform dependent declarations
 *
 * Author: Michal Lower <keton22@gmail.com>
 *
 * All rights reserved 2017
 *
 */
#ifndef __YAMC_PORT_H__
#define __YAMC_PORT_H__

/**
 *  Buffer size declarations
 */

///Payload buffer length, any payload longer than this will not be processed
#define YAMC_RX_PKT_MAX_LEN 1024


/**
 * Debug macros
 */

#ifdef YAMC_DEBUG

#include <stdio.h>

///debug log macro
#define YAMC_LOG_DEBUG_INTERNAL(...) do \
	{ \
	printf(__VA_ARGS__); \
	fflush(stdout); \
	} while(0)

///error log macro
#define YAMC_LOG_ERROR_INTERNAL(...) fprintf(stderr, __VA_ARGS__)

#else
#define YAMC_LOG_DEBUG_INTERNAL(...)
#define YAMC_LOG_ERROR_INTERNAL(...)
#endif

/**
 * Unused parameter macro
 */
#define YAMC_UNUSED_PARAMETER(x) (void) x

/**
 * Assert mechanism
 */
#include <assert.h>
#define YAMC_ASSERT(...) assert(__VA_ARGS__)

#endif
