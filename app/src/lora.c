#include "lora.h"
#include <zephyr.h>

#include <logging/log.h>
#include <device.h>
#include "global.h"
#include <pinmux/stm32/pinmux_stm32.h>
#include <misc/byteorder.h>
#include "app_rtc.h"
#include "app_adc.h"

LOG_MODULE_REGISTER(lora, LOG_LEVEL_INF);

#ifdef CONFIG_LORA_IM881A
#include <wimod_lorawan_api.h>
#endif

#include <gpio.h>

const char *nws2str(uint8_t state)
{
	switch (state)
	{
		case 0x00:
			return "INACTIVE";
		case 0x01:
			return "ative (ABP)";
		case 0x02:
			return "ative (OTAA)";
		case 0x03:
			return "joining (OTAA)";
		default:
			return "impossibru";
	}
}

int lora_init()
{
	int status;

	lora_on();
	k_sleep(K_MSEC(200));

	LOG_DBG("lora_on");

	status = wimod_lorawan_init();
	if (status)
	{
		LOG_ERR("wimod_lorawan_init failed");
		return -EIO;
	}

	status = wimod_lorawan_reset();

	for (int i = 0; i < 5; i++)
	{
		if (status)
		{
			LOG_ERR("wimod_lorawan_reset failed, turn it OFF then ON");
			lora_off();
			k_sleep(K_SECONDS(1));
			lora_on();
			k_sleep(K_MSEC(500));
			status = wimod_lorawan_send_ping();
			if (status)
			{
				LOG_ERR("hard reset failed");
			}
		}
	}

	for (;;)
	{
		struct lw_net_status_t nws;

		int status = wimod_lorawan_get_nwk_status(&nws);
		if (status)
		{
			LOG_DBG("wimod_lorawan_get_nwk_status command refused");
			k_sleep(100);
			continue;
		}

		LOG_INF("state: 0x%02"PRIu8" (%s)", nws.state, nws2str(nws.state));

		switch (nws.state)
		{
			case 0x03:
				LOG_INF("Waiting for OTAA, may be very long on TTN.");
			break;

			case 0x01:
			case 0x02:
				LOG_INF("lora connected!");
				return 0;
			break;

			case 0x00:
				LOG_INF("lora not configured, configure using lora_set_apb or lora_set_otaa");
			break;
		}

		k_sleep(K_SECONDS(10));
	}

	return 0;
}

#define INPUT_NOPULL (STM32_MODER_INPUT_MODE | STM32_PUPDR_NO_PULL)
#define INPUT_PULL_DOWN (STM32_MODER_INPUT_MODE | STM32_PUPDR_PULL_DOWN)
#define INPUT_PULL_UP (STM32_MODER_INPUT_MODE | STM32_PUPDR_PULL_UP)

static const struct pin_config pinconf_lora_uart_on[] = {
	{STM32_PIN_PA9, STM32L4X_PINMUX_FUNC_PA9_USART1_TX},
	{STM32_PIN_PA10, STM32L4X_PINMUX_FUNC_PA10_USART1_RX},
};

static const struct pin_config pinconf_lora_uart_off[] = {
	{STM32_PIN_PA9, INPUT_PULL_DOWN},
	{STM32_PIN_PA10, INPUT_PULL_DOWN},
};

void lora_off()
{
	int ret;
	struct device *dev;

	/* Power off radio module */
	dev = device_get_binding(DT_GPIO_STM32_GPIOD_LABEL);
	ret = gpio_pin_configure(dev, 2, (GPIO_DIR_OUT));
	ret = gpio_pin_write(dev, 2, 0);

	stm32_setup_pins(pinconf_lora_uart_off, ARRAY_SIZE(pinconf_lora_uart_off));
}

void lora_on()
{
	int ret;
	struct device *dev;

	/* Power on radio module */
	dev = device_get_binding(DT_GPIO_STM32_GPIOD_LABEL);
	ret = gpio_pin_configure(dev, 2, (GPIO_DIR_OUT));
	ret = gpio_pin_write(dev, 2, 1);

	stm32_setup_pins(pinconf_lora_uart_on, ARRAY_SIZE(pinconf_lora_uart_on));
}

static uint8_t *fillbuff(uint8_t *dst, const void *src, size_t size)
{
	memcpy(dst, src, size);
	return dst+size;
}

void lora_send_info(void)
{
	uint32_t vbat = adc_measure_vbat();
	uint32_t vin = adc_measure_charger();
	int32_t temp = adc_measure_temp();
	uint32_t boot_count = global.config.boot_count;

	uint32_t period[2] =
	{
		global.config.sensor_config[0].period,
		global.config.sensor_config[1].period,
	};

	// little endian protocol
	LOG_DBG("vbat:%"PRIu32" mv", vbat);
	LOG_DBG("vin:%"PRIu32" mv", vin);
	LOG_DBG("temp:%"PRId32".%"PRId32" °C", temp/10, temp > 0 ? temp%10 : -temp % 10);
	LOG_DBG("period0:%"PRIu32"", period[0]);
	LOG_DBG("period1:%"PRIu32"", period[1]);
	vbat = sys_cpu_to_le32(vbat);
	vin = sys_cpu_to_le32(vin);
	temp = sys_cpu_to_le32(temp);
	boot_count = sys_cpu_to_le32(boot_count);
	for (int i = 0 ; i < ARRAY_SIZE(period); i++)
	{
		period[i] = sys_cpu_to_le32(period[i]);
	}

	u8_t data[1+sizeof(vbat)+sizeof(vin)+sizeof(temp)+sizeof(period)+sizeof(boot_count)];
	uint8_t *ptr = &data[0];
	ptr = fillbuff(ptr, &(uint8_t){0x04}, 1);
	ptr = fillbuff(ptr, &vbat, sizeof(vbat));
	ptr = fillbuff(ptr, &temp, sizeof(temp));
	ptr = fillbuff(ptr, &vin, sizeof(vin));
	ptr = fillbuff(ptr, &period[0], sizeof(period));
	ptr = fillbuff(ptr, &boot_count, sizeof(boot_count));

	struct lw_tx_result_t txr;
	wimod_lorawan_send_u_radio_data(3, data, sizeof(data), &txr);
}

void lora_time_AppTimeReq(u8_t AnsRequired)
{
	uint32_t time = app_rtc_get();

	// unix ts to gps
	time -= 315964811;

	LOG_DBG("time_gps:%"PRIu32, time);
	LOG_DBG("time_net:0x%"PRIx32, time);

	LOG_DBG("time:0x%"PRIx32, time);
	time = sys_cpu_to_le32(time);
	LOG_DBG("time_net:0x%"PRIx32, time);

	global.lora_TokenReq++;
	global.lora_TokenReq&=0xf;

	u8_t data[6];
	data[0] = 0x01; // cid
	memcpy(&data[1], &time, sizeof(time));
	data[5] = (AnsRequired ? (1 << 4) : 0) | (global.lora_TokenReq & 0xf);

	struct lw_tx_result_t txr;
	wimod_lorawan_send_u_radio_data(202, data, sizeof(data), &txr);
}

void lora_time_AppTimeAns(const uint8_t data[5])
{
	uint32_t time;

	uint8_t token = data[4] & 0xf;

	if (token != global.lora_TokenReq)
	{
		LOG_DBG("ignoring:request token:0x%"PRIx8", answer token:0x%"PRIx8, global.lora_TokenReq, token);
		return;
	}
	else
	{
		LOG_DBG("ok:request token:0x%"PRIx8", answer token:0x%"PRIx8, global.lora_TokenReq, token);
	}

	memcpy(&time, data, sizeof(time));

	LOG_DBG("time_net:0x%"PRIx32, time);
	time = sys_le32_to_cpu(time);
	LOG_DBG("time:0x%"PRIx32, time);
	LOG_INF("time delta : %"PRId32, time);
	if (time)
	{
		global.lora_time_delta = time;
		global.lora_time_changed = 1;
	}
}

static int decode_config(uint32_t port, const uint8_t *data, size_t size)
{
	struct sensor_config_t *c;
	printk("toto1 size=%d\n", size);
	switch (port)
	{
		case 1:
		c = &global.config.sensor_config[0];
		break;

		case 2:
		c = &global.config.sensor_config[1];
		break;

		default:
		return -EINVAL;
	}

	if (size < 9)
	{
		LOG_WRN("size < 9 ignoring");
		return -EINVAL;
	}

	uint8_t version = data[0];

	if (version != 3 && version !=4)
	{
		LOG_WRN("version %"PRIu8"unkown", version);
		return -EINVAL;
	}

	uint32_t period;
	uint32_t wakeup_ms;
	memcpy(&period, &data[1], sizeof(period));
	memcpy(&wakeup_ms, &data[5], sizeof(wakeup_ms));
	period = sys_le32_to_cpu(period);
	wakeup_ms = sys_le32_to_cpu(wakeup_ms);

	c->period = period;
	c->wakeup_ms = wakeup_ms;
	if (c->period == 0)
	{
		c->enable = 0;
	}

	global.config_changed = 1;

	if (c->enable)
	{
		LOG_INF("input%"PRIu32": period:%"PRIu32" s, wakeup_ms:%"PRIu32" ms", port - 1, c->period, c->wakeup_ms);
	}
	else
	{
		LOG_INF("input%"PRIu32": disable", port - 1);
	}
	printk("toto2\n");
	return 0;
}

int wimod_data_rx_handler(uint32_t port, const uint8_t *data, size_t size)
{
	int ret = 0;
	switch(port)
	{
		case 202:
			if (size != 6)
			{
				LOG_ERR("sync:unsupported size:%zu", size);
				break;
			}

			if (data[0] != 0x01)
			{
				LOG_ERR("sync:unsupported message type:%"PRIx8, data[0]);
				break;
			}

			lora_time_AppTimeAns(data+1);
			return 0;
		break;

		case 1:
		case 2:
			ret = decode_config(port, data, size);
			return ret;
		break;

		default:
		LOG_DBG("ignored message on port %"PRIu32, port);
	}

	return 1;
}
