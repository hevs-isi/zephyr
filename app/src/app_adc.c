#include <adc.h>
#include <zephyr.h>
#include <misc/printk.h>
#include <pinmux/stm32/pinmux_stm32.h>
#include <soc.h>
#include <gpio.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(adc, LOG_LEVEL_DBG);

#include "app_adc.h"
uint16_t adc_data[1];

static const struct adc_channel_cfg channel_cfg[] =
{
	{
		.gain				= ADC_GAIN_1,
		.reference			= ADC_REF_INTERNAL,
		.acquisition_time	= ADC_ACQ_TIME(ADC_ACQ_TIME_TICKS, 641),
		.channel_id	   		= 0,
		.differential		= 0,
	},
	{
		.gain				= ADC_GAIN_1,
		.reference			= ADC_REF_INTERNAL,
		.acquisition_time	= ADC_ACQ_TIME(ADC_ACQ_TIME_TICKS, 641),
		.channel_id	   		= 1,
		.differential		= 0,
	},
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
	{
		.gain				= ADC_GAIN_1,
		.reference			= ADC_REF_INTERNAL,
		.acquisition_time	= ADC_ACQ_TIME(ADC_ACQ_TIME_TICKS, 641),
		.channel_id	   		= 4,
		.differential		= 0,
	},
};

const struct adc_sequence sequence[] =
{
	{
		.channels	= BIT(0),
		.buffer	  = adc_data,
		.buffer_size = sizeof(adc_data),
		.resolution  = 12,
	},
	{
		.channels	= BIT(1),
		.buffer	  = adc_data,
		.buffer_size = sizeof(adc_data),
		.resolution  = 12,
	},
	{
		.channels	= BIT(2),
		.buffer	  = adc_data,
		.buffer_size = sizeof(adc_data),
		.resolution  = 12,
	},
	{
		.channels	= BIT(3),
		.buffer	  = adc_data,
		.buffer_size = sizeof(adc_data),
		.resolution  = 12,
	},
	{
		.channels	= BIT(4),
		.buffer	  = adc_data,
		.buffer_size = sizeof(adc_data),
		.resolution  = 12,
	},
};

static const struct pin_config pinconf[] =
{
	{STM32_PIN_PC0, STM32L4X_PINMUX_FUNC_PC0_ADC123_IN1},
	{STM32_PIN_PC1, STM32L4X_PINMUX_FUNC_PC1_ADC123_IN2},
	{STM32_PIN_PC2, STM32L4X_PINMUX_FUNC_PC2_ADC123_IN3},
	{STM32_PIN_PC3, STM32L4X_PINMUX_FUNC_PC3_ADC123_IN4},
};

static uint16_t vdda_mv(uint16_t data)
{
	return (uint32_t)3000*(*VREFINT_CAL_ADDR)/data;
}


/**
 * \brief Convert an AD convertsion result to milivolts
 *
 * \param ref, the ADC conversion result of VREFINT
 * \pram in, the ADC conversion result of the input
 *
 * \return the result in milivolts
 *
 * \see Chapter 18.4.34 in RM0351 for the computation.
 */
static uint16_t mes_mv(uint16_t ref, uint16_t in)
{
	uint64_t tmp;

	/*
	 * For maximum precision, all multipications are done before dividing.
	 */
	tmp = 3000;
	tmp *= (*VREFINT_CAL_ADDR);
	tmp *= in;

	tmp /= ref;
	tmp /= 4095;

	return tmp;
}

static struct device *adc_dev;

static uint16_t mes(const struct adc_sequence *s)
{
	uint16_t reference;
	uint16_t in;
	adc_read(adc_dev, &sequence[0]);
	reference = adc_data[0];
	adc_read(adc_dev, s);
	in = adc_data[0];
	LOG_DBG("ref:%"PRIu16" in: %"PRIu16, reference, in);
	return mes_mv(reference, in);
}

uint16_t adc_measure_vbat(void)
{
	uint16_t tmp;
	struct device *pin = device_get_binding(DT_GPIO_STM32_GPIOA_LABEL);
	gpio_pin_configure(pin, 8, (GPIO_DIR_OUT));
	gpio_pin_write(pin, 8, 0);
	tmp = mes(&sequence[1]);
	gpio_pin_configure(pin, 8, (GPIO_DIR_IN));
	return tmp*133/33;
}

uint16_t adc_measure_charger(void)
{
	return mes(&sequence[4]);
}

static uint16_t adc_measure_sensor0(void)
{
	return mes(&sequence[2]);
}

static uint16_t adc_measure_sensor1(void)
{
	return mes(&sequence[3]);
}

uint16_t adc_measure_sensor(uint32_t sensor)
{
	switch (sensor)
	{
		case 0: return adc_measure_sensor0();
		case 1: return adc_measure_sensor0();
		default:
		return UINT16_MAX;
	}
}


void adc_init(void)
{
	adc_dev = device_get_binding(DT_ADC_1_NAME);
	stm32_setup_pins(pinconf, ARRAY_SIZE(pinconf));

	for (size_t i = 0; i < ARRAY_SIZE(channel_cfg); i++)
	{
		adc_channel_setup(adc_dev, &channel_cfg[i]);
	}

	adc_read(adc_dev, &sequence[0]);
	LOG_INF("VREFINT_DATA=%"PRIu16", VDDA=%"PRIu16, *VREFINT_CAL_ADDR, vdda_mv(adc_data[0]));
}

void adc_test(void)
{
	adc_read(adc_dev, &sequence[0]);
	LOG_INF("VDDA=%"PRIu16"\n", vdda_mv(adc_data[0]));
	LOG_INF("adc_measure_vbat():%"PRIu16" mv", adc_measure_vbat());
	LOG_INF("adc_measure_charger():%"PRIu16" mv", adc_measure_charger());
	LOG_INF("adc_measure_sensor0():%"PRIu16" mv", adc_measure_sensor0());
	LOG_INF("adc_measure_sensor1():%"PRIu16" mv", adc_measure_sensor1());
}
