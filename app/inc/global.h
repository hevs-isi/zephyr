/*************************************************************************//**
 * \file gloabl.h
 * \brief board include selection
 *
 * \author Marc Pignat
 * \copyright Copyright (c) 2019 Marc Pignat
 *
 **************************************************************************//*
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef GLOBAL_H
#define GLOBAL_H

#include "saved_config.h"

/**
 * \brief LED blink description
 */
struct led_t
{
	uint32_t t_on_ms;
	uint32_t t_off_ms;
};

/**
 * \brief Holds the global state of the application
 */
struct global_t
{
	/**
	 * LoRaWAN Application Layer Clock Synchronization token
	 */
	u8_t lora_TokenReq;

	/**
	 * sleep prevent flag, set by button interrupt
	 */
	volatile u32_t sleep_prevent;

	/**
	 * Transmit request flag, set by button interrupt
	 */
	volatile u32_t tx_now;

	/**
	 * sleep permit flag, set by the console
	 */
	volatile u32_t sleep_permit;

	/**
	 * In memory copy of the configuration variable stored in non-volatile
	 * memory.
	 */
	struct saved_config_t config;

	/**
	 * flag, when the the config must be stored to flash.
	 */
	volatile uint32_t config_changed;

	/**
	 * flag, set to inform the RTC has changed significantly (not just adjusted)
	 */
	volatile uint32_t rtc_reset;

	/**
	 * The value of the lora time delta
	 */
	volatile uint32_t lora_time_delta;

	/**
	 * flag, lora_time_delta is now valid
	 */
	volatile uint32_t lora_time_changed;

	/**
	 * State of LED0
	 */
	struct led_t led0;

	/**
	 * State of LED1
	 */
	struct led_t led1;
};

#ifdef __cplusplus
extern "C"
{
#endif

extern struct global_t global;

#ifdef __cplusplus
}
#endif

#endif /* GLOBAL_H */
