/*
 * YAMC - Yet Another MQTT Client library
 *
 * yamc_port.h - Platform dependent declarations
 *
 * Author: Michal Lower <https://github.com/keton>
 * 
 * Licensed under MIT License (see LICENSE file in main repo directory)
 *
 */
#ifndef __YAMC_PORT_H__
#define __YAMC_PORT_H__

/*************************
 *
 * Buffer size declaration
 *
 *************************/

/// Payload buffer length, any payload longer than this will not be processed
#define YAMC_RX_PKT_MAX_LEN 1024

/*************************
 *
 * Debug macros
 *
 *************************/

#include <stdio.h>

/// print string to debug output (i.e. stdout)
#define YAMC_DEBUG_PRINTF(...) \
	do                         \
	{                          \
		printf(__VA_ARGS__);   \
		fflush(stdout);        \
	} while (0)

/// print string to error output (i.e. stderr)
#define YAMC_ERROR_PRINTF(...) fprintf(stderr, __VA_ARGS__)

/*************************
 *
 * Assert mechanism
 *
 *************************/
#include <assert.h>
#define YAMC_ASSERT(...) assert(__VA_ARGS__)

/**************************
 *
 * Unused parameter macro
 *
 **************************/
#define YAMC_UNUSED_PARAMETER(x) (void)x

#endif
