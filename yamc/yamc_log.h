/*
 * YAMC - Yet Another MQTT Client library
 *
 * yamc_log.h - debug log module
 *
 * Author: Michal Lower <keton22@gmail.com>
 *
 * All rights reserved 2017
 *
 */

#ifndef __YAMC_LOG_H__
#define __YAMC_LOG_H__

#ifdef YAMC_DEBUG

///debug log macro
#define LOG_DEBUG(...) do \
	{ \
	printf(__VA_ARGS__); \
	fflush(stdout); \
	} while(0)

///error log macro
#define LOG_ERROR(...) fprintf(stderr, __VA_ARGS__)

void yamc_log_hex(const uint8_t* const p_buff, const uint32_t buff_len);
void yamc_log_raw_pkt(const yamc_instance_t* const p_instance);

#else

#define LOG_DEBUG(...)
#define LOG_ERROR(...)
#define yamc_log_hex(...)
#define yamc_log_raw_pkt(...)

#endif

#endif /* YAMC_LOG_H_ */
