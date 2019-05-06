#include <adc.h>
#include <zephyr.h>
#include <misc/printk.h>
#include <pinmux/stm32/pinmux_stm32.h>
#include <gpio.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(adc, LOG_LEVEL_DBG);

#include "app_adc.h"
uint16_t temp_data[8];

static const struct adc_channel_cfg channel_cfg[] =
{
	{
		.gain				= ADC_GAIN_1,
		.reference			= ADC_REF_INTERNAL,
		.acquisition_time	= ADC_ACQ_TIME(ADC_ACQ_TIME_TICKS, 641),
		.channel_id	   		= 2,
		.differential		= 0,
	},
	{
		.gain				= ADC_GAIN_1,
		.reference			= ADC_REF_INTERNAL,
		.acquisition_time	= ADC_ACQ_TIME(ADC_ACQ_TIME_TICKS, 641),
		.channel_id	   		= 3,
		.differential		= 0,
	},
};

const struct adc_sequence sequence[] =
{
	{
		.channels	= BIT(2),
		.buffer	  = temp_data,
		.buffer_size = sizeof(temp_data),
		.resolution  = 12,
	},
	{
		.channels	= BIT(3),
		.buffer	  = temp_data,
		.buffer_size = sizeof(temp_data),
		.resolution  = 12,
	},
};

static const struct pin_config pinconf[] =
{
	{STM32_PIN_PC1, STM32L4X_PINMUX_FUNC_PC1_ADC123_IN2},
	{STM32_PIN_PC2, STM32L4X_PINMUX_FUNC_PC2_ADC123_IN3},
};

void adc_test(void)
{
	int ret;

	struct device *adc_dev = device_get_binding(DT_ADC_1_NAME);
	struct device *dev = device_get_binding(DT_GPIO_STM32_GPIOA_LABEL);
	ret = gpio_pin_configure(dev, 8, (GPIO_DIR_OUT));
	gpio_pin_write(dev, 8, 1);

	// FIXME : try to enable gpioc
	//struct device *dev = device_get_binding(DT_GPIO_STM32_GPIOC_LABEL);
	//ret = gpio_pin_configure(dev, 0, (GPIO_DIR_OUT));
	// FIXME end

	stm32_setup_pins(pinconf, ARRAY_SIZE(pinconf));


	LOG_DBG("adc_dev = %p", adc_dev);

	for (size_t i = 0; i < ARRAY_SIZE(channel_cfg); i++)
	{
		adc_channel_setup(adc_dev, &channel_cfg[i]);
	}

	while (1) {
		for (size_t i = 0 ; i < ARRAY_SIZE(sequence); i++)
		{
			ret = adc_read(adc_dev, &sequence[i]);
			k_sleep(100);

			if (ret != 0)
			{
				printk("adc_read() failed with code %d", ret);
			} else {
				printk("adc_read() value PC%d : %d\n", (int)i+1, temp_data[0]);
			}
		}
		k_sleep(1000);
	}
}
