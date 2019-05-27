#include "lora.h"
#include <zephyr.h>

#include <logging/log.h>
#include <device.h>
#include "global.h"
#include <pinmux/stm32/pinmux_stm32.h>
#include <misc/byteorder.h>
#include "app_rtc.h"
#include "app_adc.h"

LOG_MODULE_REGISTER(lora, LOG_LEVEL_DBG);

#ifdef CONFIG_LORA_IM881A
#include <wimod_lorawan_api.h>
#endif

#ifdef CONFIG_BOARD_NRF52840_LORAIOT
#include <board_utils.h>
#endif

#include <gpio.h>

static struct device *uart;
static struct uart_device_config *uart_cfg;

K_THREAD_STACK_DEFINE(lora_msg_stack, 1024);
static struct k_work_q lora_msg_work_q;

struct lora_msg {
	u8_t port;
	u8_t data[20];
	u8_t size;
	struct k_work work;
};

static struct lora_msg lmsg;

#include <uart.h>

void join_callback()
{
	LOG_INF("LoRaWAN Network started.\n");
	//blink_led(LED_GREEN, MSEC_PER_SEC/4, K_SECONDS(3));
	#ifdef CONFIG_BOARD_NRF52840_LORAIOT
		VDDH_DEACTIVATE();
	#endif
}

void disable_uart()
{
	#ifdef CONFIG_BOARD_NRF52840_LORAIOT
	if( NOT_IN_DEBUG() ){
		*(uart_cfg->base + 0x500) = 0;
	}
	#endif
}

void enable_uart()
{
	#ifdef CONFIG_BOARD_NRF52840_LORAIOT
	if( NOT_IN_DEBUG() ){
		*(uart_cfg->base + 0x500) = 4UL;
	}
	#endif
}

static void lora_msg_send(struct k_work *item)
{
	struct lora_msg *msg = CONTAINER_OF(item, struct lora_msg, work);

	enable_uart();
	wimod_lorawan_send_c_radio_data(msg->port, msg->data, msg->size);
	disable_uart();
	#ifdef CONFIG_BOARD_NRF52840_LORAIOT
	VDDH_DEACTIVATE();
	#endif
}

void lora_init()
{
	uart = device_get_binding(CONFIG_LORA_IM881A_UART_DRV_NAME);
	uart_cfg = (struct uart_device_config *)(uart->config->config_info);

	/* BUG: couldn't launch the debug when the function 'uart_irq_rx_enable' is called too quickly */
	k_busy_wait(1000000);

	#ifdef CONFIG_BOARD_NRF52840_LORAIOT
	VDDH_ACTIVATE();
	#endif

	wimod_lorawan_init();

	k_work_q_start(&lora_msg_work_q, lora_msg_stack,
		   K_THREAD_STACK_SIZEOF(lora_msg_stack), K_PRIO_PREEMPT(3));

	k_work_init(&lmsg.work, lora_msg_send);

	// im881 modules comes from factory as class C devices, this must be set
	wimod_lorawan_set_rstack_config();

	wimod_lorawan_join_network_request(join_callback);

	disable_uart();
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

void lora_send_info(void)
{
	uint32_t value = adc_measure_vbat();

	// little endian protocol
	LOG_DBG("value:%"PRIu32, value);
	value = sys_cpu_to_le32(value);

	u8_t data[1+sizeof(uint32_t)+sizeof(uint32_t)];
	data[0] = 0x01; // version
	memcpy(&data[1], &value, sizeof(value));

	wimod_lorawan_send_u_radio_data(3, data, sizeof(data));
}

void lora_time_AppTimeReq(u8_t AnsRequired)
{
	uint32_t time = app_rtc_get();

	// unix ts to gps
	time -= 315964800;

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

	wimod_lorawan_send_u_radio_data(202, data, sizeof(data));
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
	LOG_DBG("time delta : %"PRId32, time);
	if (time)
	{
		global.lora_time_delta = time;
		global.lora_time_changed = 1;
	}
}

int wimod_data_rx_handler(uint32_t port, const uint8_t *data, size_t size)
{
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

		LOG_DBG("ignored message on port %"PRIu32, port);
	}

	return 1;
}
