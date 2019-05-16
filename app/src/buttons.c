#include <adc.h>
#include <zephyr.h>
#include <misc/printk.h>
#include <gpio.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(buttons, LOG_LEVEL_DBG);

#include "buttons.h"
#include "global.h"
#include "stm32_lp.h"

static void button1_handler(struct device *gpio, struct gpio_callback *cb, u32_t pins)
{
	u32_t value;
	gpio_pin_read(gpio, 11, &value);
	if (!value)
	{
		global.tx_now = 1;
	}
}

static void button2_handler(struct device *gpio, struct gpio_callback *cb, u32_t pins)
{
	u32_t value;
	gpio_pin_read(gpio, 15, &value);
	if (!value)
	{
		lp_sleep_prevent();
	}
}

static struct gpio_callback cb[2];

static void button_init(const char *port, uint32_t pin, struct gpio_callback *cb, void(*handler)(struct device *gpio, struct gpio_callback *cb, u32_t pins))
{
	struct device *dev;
	int ret;

	dev = device_get_binding(port);
	if (!dev) {
		printk("error\n");
		return;
	}

	ret = gpio_pin_configure(dev, pin,
			   GPIO_DIR_IN | GPIO_INT | GPIO_PUD_PULL_UP | GPIO_INT_EDGE | GPIO_INT_DOUBLE_EDGE);
	if (ret)
	{
		LOG_DBG("gpio_pin_configure failed:%d", ret);
	}

	gpio_init_callback(cb, handler, BIT(pin));

	ret = gpio_add_callback(dev, cb);
	if (ret)
	{
		LOG_DBG("gpio_add_callback failed");
	}

	ret = gpio_pin_enable_callback(dev, pin);
	if (ret)
	{
		LOG_DBG("gpio_pin_enable_callback failed");
	}
}

void buttons_init(void)
{
	button_init(DT_GPIO_STM32_GPIOC_LABEL, 11, &cb[0], button1_handler);
	button_init(DT_GPIO_STM32_GPIOB_LABEL, 15, &cb[1], button2_handler);
}
