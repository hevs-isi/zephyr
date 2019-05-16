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

struct led_t
{
	uint32_t t_on_ms;
	uint32_t t_off_ms;
};

struct global_t
{
	u8_t lora_TokenReq;
	volatile u32_t sleep_prevent;
	volatile u32_t tx_now;
	volatile u32_t sleep_permit;
	struct saved_config_t config;
	volatile uint32_t config_changed;
	volatile uint32_t rtc_reset;
	volatile uint32_t lora_time_delta;
	volatile uint32_t lora_time_changed;
	struct led_t led0;
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
