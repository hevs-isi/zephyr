/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <board_utils.h>
#include "shell_lora.h"
#include <kernel.h>
#include <device.h>
#include "stm32_lp.h"

#include <string.h>

#include "lora.h"
#include "gps.h"
#include "saved_config.h"
#include "global.h"
#include <sensor.h>
#include "app_adc.h"
#include "app_rtc.h"
#include "buttons.h"
#include <wimod_lorawan_api.h>

struct global_t global =
{
	.lora_TokenReq	= 0,
	.sleep_prevent	= 0,
	.sleep_permit	= 0,
	.config_changed	= 0,
	.led0			=
	{
		.t_on_ms = 3,
		.t_off_ms = 997,
	},
	.led1			=
	{
		.t_on_ms = 3,
		.t_off_ms = 330,
	},
};

#include <logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#if CONFIG_LORA_IM881A
#include <im881a.h>
#endif

#include <device.h>
#include <gpio.h>
#include <crc.h>

static struct device *led0_dev;
static struct device *led1_dev;

static void leds_init(void)
{
	led0_dev = device_get_binding(LED0_GPIO_CONTROLLER);
	led1_dev = device_get_binding(LED1_GPIO_CONTROLLER);
	gpio_pin_configure(led0_dev, LED0_GPIO_PIN, GPIO_DIR_OUT);
	gpio_pin_configure(led1_dev, LED1_GPIO_PIN, GPIO_DIR_OUT);

	gpio_pin_write(led0_dev, LED0_GPIO_PIN, 0);
	gpio_pin_write(led1_dev, LED1_GPIO_PIN, 0);
}

static void led0_set(uint32_t value)
{
	gpio_pin_write(led0_dev, LED0_GPIO_PIN, value);
}

static void led1_set(uint32_t value)
{
	gpio_pin_write(led1_dev, LED1_GPIO_PIN, value);
}

#include <kernel.h>
#include <arch/cpu.h>
#include <misc/__assert.h>
#include <soc.h>
#include <init.h>
#include <uart.h>
#include <clock_control.h>

struct periodic_timer_t
{
	const char *name;
	uint32_t next;
	uint32_t period;
	uint32_t enable;
	uint32_t configured;
};

static uint32_t expired(const struct periodic_timer_t *t, uint32_t now)
{
	int32_t delta = (t->next - now);

	if (t->enable && t->configured)
	{
		//LOG_DBG("timer:%s now:%"PRIu32" next:%"PRIu32" delta:%"PRId32, t->name, now, t->next, delta);
	}

	return (delta < 0) && t->enable && t->configured;
}

static uint32_t expiry(const struct periodic_timer_t *t, uint32_t now)
{
	int32_t delta = (t->next - now);

	if (!t->enable)
	{
		return INT32_MAX;
	}

	if (expired(t, now))
	{
		return 0;
	}

	return delta;
}

static uint32_t next_timer_value(uint32_t now, uint32_t period, uint32_t pr)
{
	return (((now / period) + 1) * period) + (pr % period);
}

static uint32_t expired_restart_psnr(struct periodic_timer_t *t, uint32_t now, uint32_t pr)
{
	uint32_t ex = 0;

	if (expired(t, now))
	{
		ex = 1;
	}

	if (ex == 1 || (t->enable && t->configured == 0))
	{
		t->next = next_timer_value(now, t->period, pr);
		int32_t delta = (t->next - now);
		LOG_DBG("%s next:%"PRIu32" delta:%"PRId32, t->name, t->next, delta);
		t->configured = 1;
	}

	return ex;
}

static uint32_t expired_restart(struct periodic_timer_t *t, uint32_t now)
{
	return expired_restart_psnr(t, now, 0);
}

static struct periodic_timer_t measure_timer_a =
{
	.name = "measure_timer_a",
	.next = 0,
	.period = 5*60,
	.enable = 1,
};

static struct periodic_timer_t measure_timer_b =
{
	.name = "measure_timer_b",
	.next = 0,
	.period = 10*60,
	.enable = 1,
};

static struct periodic_timer_t tx_timer_a =
{
	.name = "tx_timer_a",
	.next = 0,
	.period = 15*60,
	.enable = 1,
};

static struct periodic_timer_t tx_timer_b =
{
	.name = "tx_timer_b",
	.next = 0,
	.period = 15*60,
	.enable = 1,
};

static struct periodic_timer_t sync_timer =
{
	.name = "sync_timer",
	.next = 0,
	.period = 6*60*60,
	.enable = 1,
};

static struct periodic_timer_t charge_timer =
{
	.name = "charge_timer",
	.next = 0,
	.period = 10*60,
	.enable = 1,
};

static struct periodic_timer_t info_timer =
{
	.name = "info_timer",
	.next = 0,
	.period = 6*60*60,
	.enable = 1,
};

struct periodic_timer_t *timers[] =
{
	&measure_timer_a,
	&measure_timer_b,
	&tx_timer_a,
	&tx_timer_b,
	&charge_timer,
	&sync_timer,
	&info_timer,
};

static uint32_t expiry_min(struct periodic_timer_t **ts, size_t nr, uint32_t now)
{
	uint32_t min = INT32_MAX;

	for (int i = 0 ; i < nr ; i++)
	{
		uint32_t ex = expiry(ts[i], now);

		if (ex < min)
		{
			min = ex;
		}
	}

	return min;
}

static void init_timer(struct periodic_timer_t *m, struct periodic_timer_t *t, const struct sensor_config_t *c)
{
	m->enable = c->enable;
	m->period = c->period;
	m->configured = 0;
	t->enable = c->enable;
	t->period = c->period*c->tx_period;
	t->configured = 0;
}

static void init_from_config(const struct saved_config_t *c)
{
	init_timer(&measure_timer_a, &tx_timer_a, &c->a);
	init_timer(&measure_timer_b, &tx_timer_b, &c->b);
}

static void all_timers_now(uint32_t now)
{
	for (size_t i = 0 ; i < ARRAY_SIZE(timers);i++)
	{
		if (timers[i]->enable)
		{
			timers[i]->next = now + 10;
			timers[i]->configured = 1;
		}
	}
}

static uint32_t prng(uint32_t next, const uint8_t *devaddr, size_t size)
{
	u8_t data[sizeof(next)+size];
	memcpy(&data[0], devaddr, size);
	memcpy(&data[size], &next, sizeof(next));
	return crc32_ieee(data, sizeof(data));
}

static int measure(const struct sensor_config_t *c, uint32_t *value, uint32_t nr)
{
	int ret = 0;
	*value = UINT32_MAX;

	switch((enum psu_e)c->psu)
	{
		case PSU_5V:
			LOG_DBG("measure using PSU_5V");
			psu_5v(1);
		break;

		case PSU_INDUS:
			LOG_DBG("measure using PSU_INDUS");
			psu_ind(1);
		break;

		case PSU_NONE:
			LOG_DBG("measure without PSU");
		default:
			LOG_DBG("c->psu invalid");
			ret = -ENOTSUP;
	}

	LOG_DBG("wait %"PRIu32" ms", c->wakeup_ms);
	k_sleep(c->wakeup_ms);

	switch((enum mode_e)c->mode)
	{
		case MODE_0_10V:
			*value = adc_measure_sensor(nr);
			LOG_DBG("measure ch%"PRIu32" : %"PRIu32" mV", nr, *value);
		break;

		default:
		LOG_ERR("not yet implemented : sensor_config->mode : %"PRIu8, c->mode);
		ret = -ENOTSUP;
		goto end_mes;
	}

end_mes:
	switch((enum psu_e)c->psu)
	{
		case PSU_5V:
			psu_5v(0);
		break;

		case PSU_INDUS:
			psu_ind(0);
		break;

		case PSU_NONE:
		default:
		break;
	}

	return ret;
}

static uint32_t tick(uint32_t now)
{
	/* FIXME : wait for end of TX, or schedule TX far appart */
	uint32_t RADIO_TIMEOUT = (10*1000);
	u8_t devaddr[6] = {1,2,3,4,5,6}; // FIMXE
	int ret;
	uint32_t value0;
	uint32_t value1;

	if (expired_restart(&measure_timer_a, now))
	{
		LOG_DBG("expired:measure_timer_a");
		ret = measure(&global.config.a, &value0, 0);
	}

	if (expired_restart(&measure_timer_b, now))
	{
		LOG_DBG("expired:measure_timer_b");
		ret = measure(&global.config.b, &value1, 1);
	}

	if (expired_restart_psnr(&tx_timer_a, now, prng(tx_timer_a.next, devaddr, sizeof(devaddr))))
	{
		LOG_DBG("expired:tx_timer_a");
		wimod_lorawan_send_u_radio_data(1, &value0, sizeof(value0));
		k_sleep(RADIO_TIMEOUT);
	}

	if (expired_restart_psnr(&tx_timer_b, now, prng(tx_timer_b.next, devaddr, sizeof(devaddr))))
	{
		LOG_DBG("expired:tx_timer_b");
		wimod_lorawan_send_u_radio_data(2, &value1, sizeof(value1));
		k_sleep(RADIO_TIMEOUT);
	}

	if (expired_restart_psnr(&sync_timer, now, prng(sync_timer.next, devaddr, sizeof(devaddr))))
	{
		LOG_DBG("expired:sync_timer");
		lora_time_AppTimeReq(0);
		k_sleep(RADIO_TIMEOUT);
	}

	if (expired_restart(&charge_timer, now))
	{
		LOG_DBG("expired:charge_timer");
		// FIXME : measure battery temperature
		psu_charge(1);
	}

	if (expired_restart_psnr(&info_timer, now, prng(info_timer.next, devaddr, sizeof(devaddr))))
	{
		LOG_DBG("expired:info_timer");
		lora_send_info();
		k_sleep(RADIO_TIMEOUT);
	}
	uint32_t sleep = expiry_min(timers, ARRAY_SIZE(timers), now);

	return sleep;
}
#define STACKSIZE 1024

void led0_thread(void *u1, void *u2, void *u3)
{
	ARG_UNUSED(u1);
	ARG_UNUSED(u2);
	ARG_UNUSED(u3);

	for(;;)
	{
		if (global.led0.t_on_ms)
		{
			led0_set(1);
			k_sleep(global.led0.t_on_ms);
		}
		led0_set(0);
		k_sleep(global.led0.t_off_ms);
	}
}

void led1_thread(void *u1, void *u2, void *u3)
{
	ARG_UNUSED(u1);
	ARG_UNUSED(u2);
	ARG_UNUSED(u3);

	for(;;)
	{
		if (global.led1.t_on_ms)
		{
			led1_set(1);
			k_sleep(global.led1.t_on_ms);
		}
		led1_set(0);
		k_sleep(global.led1.t_off_ms);
	}
}

static K_THREAD_DEFINE(led0_thread_id, STACKSIZE, led0_thread, NULL, NULL,
	NULL, 1, 0, K_FOREVER);
static K_THREAD_DEFINE(led1_thread_id, STACKSIZE, led1_thread, NULL, NULL,
	NULL, 1, 0, K_FOREVER);

void app_main(void *u1, void *u2, void *u3)
{
	ARG_UNUSED(u1);
	ARG_UNUSED(u2);
	ARG_UNUSED(u3);

	lp_init();
	lp_sleep_prevent();
	leds_init();

	k_thread_start(led0_thread_id);
	k_thread_suspend(led0_thread_id);
	k_thread_start(led1_thread_id);
	k_thread_suspend(led1_thread_id);
	led0_set(0);
	led1_set(0);

	psu_5v(0);
	psu_ind(0);
	psu_charge(0);
	psu_cpu_hp(1);
	saved_config_init();
	saved_config_read(&global.config);
	init_from_config(&global.config);
	app_rtc_init();
	all_timers_now(app_rtc_get());
	gps_off();

	//gps_init();

	lora_on();
	lora_init();
	adc_init();
	buttons_init();

	uint32_t sleep_prevent_duration = 0;
	uint32_t sleep_seconds = 0;

	for (;;)
	{
		uint32_t now = app_rtc_get();

		if (!sleep_prevent_duration)
		{
			LOG_INF("now:%"PRIu32, now);
		}

		if (global.tx_now == 1)
		{
			LOG_INF("tx now!");
			global.tx_now = 0;
			all_timers_now(app_rtc_get());
		}

		if (global.sleep_prevent == 1)
		{
			LOG_INF("wakeup!");
			global.sleep_prevent = 0;
			sleep_prevent_duration = 60;
			k_thread_resume(led0_thread_id);
		}

		if (global.sleep_permit == 1)
		{
			global.sleep_permit = 0;
			sleep_prevent_duration = 0;
		}

		k_thread_resume(led1_thread_id);
		sleep_seconds = tick(now);
		k_thread_suspend(led1_thread_id);
		led1_set(0);

		if (global.config_changed == 1)
		{
			global.config_changed = 0;
			saved_config_save(&global.config);
			init_from_config(&global.config);
		}

		if (global.lora_time_changed == 1)
		{
			global.lora_time_changed = 0;
			app_rtc_set(app_rtc_get() + global.lora_time_delta);
		}

		if (global.rtc_reset == 1)
		{
			init_from_config(&global.config);
			all_timers_now(app_rtc_get());
		}

		if (sleep_prevent_duration > 0)
		{
			if (sleep_prevent_duration % 5 == 0)
			{
				LOG_DBG("sleep_prevent_duration : %"PRIu32" seconds", sleep_prevent_duration);
			}
			k_sleep(1000);
			sleep_prevent_duration--;
		}
		else
		{
			k_thread_suspend(led0_thread_id);
			led0_set(0);
			led1_set(0);
			if (sleep_seconds)
			{
				LOG_DBG("sleep : %"PRIu32" seconds", sleep_seconds);
				stm32_sleep(1000*sleep_seconds);
			}
		}
	}
}

K_THREAD_DEFINE(app_main_id, 4*STACKSIZE, app_main, NULL, NULL,
		NULL, 2, 0, K_NO_WAIT);
