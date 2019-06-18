#include <adc.h>
#include <zephyr.h>
#include <misc/printk.h>
#include <pinmux/stm32/pinmux_stm32.h>
#include <soc.h>
#include <gpio.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(adc, LOG_LEVEL_INF);

#include "app_adc.h"
uint16_t _adc_data[1];

enum adc_input_e
{
	ADC_INPUT_VREF_INT	= 0,
	ADC_INPUT_UNUSED	= 1,
	ADC_INPUT_SENSOR_0	= 2,
	ADC_INPUT_SENSOR_1	= 3,
	ADC_INPUT_CHARGER	= 4,
	ADC_INPUT_VBAT		= 5,
	ADC_INPUT_TEMP		= 6,
};

static const struct adc_channel_cfg channel_cfg[] =
{
	[ADC_INPUT_VREF_INT] =
	{
		.gain				= ADC_GAIN_1,
		.reference			= ADC_REF_INTERNAL,
		.acquisition_time	= ADC_ACQ_TIME(ADC_ACQ_TIME_TICKS, 641),
		.channel_id	   		= 0,
		.differential		= 0,
	},
	[ADC_INPUT_UNUSED] =
	{
		.gain				= ADC_GAIN_1,
		.reference			= ADC_REF_INTERNAL,
		.acquisition_time	= ADC_ACQ_TIME(ADC_ACQ_TIME_TICKS, 641),
		.channel_id	   		= 1,
		.differential		= 0,
	},
	[ADC_INPUT_SENSOR_0] =
	{
		.gain				= ADC_GAIN_1,
		.reference			= ADC_REF_INTERNAL,
		.acquisition_time	= ADC_ACQ_TIME(ADC_ACQ_TIME_TICKS, 641),
		.channel_id	   		= 2,
		.differential		= 0,
	},
	[ADC_INPUT_SENSOR_1] =
	{
		.gain				= ADC_GAIN_1,
		.reference			= ADC_REF_INTERNAL,
		.acquisition_time	= ADC_ACQ_TIME(ADC_ACQ_TIME_TICKS, 641),
		.channel_id	   		= 3,
		.differential		= 0,
	},
	[ADC_INPUT_CHARGER] =
	{
		.gain				= ADC_GAIN_1,
		.reference			= ADC_REF_INTERNAL,
		.acquisition_time	= ADC_ACQ_TIME(ADC_ACQ_TIME_TICKS, 641),
		.channel_id	   		= 4,
		.differential		= 0,
	},
	[ADC_INPUT_VBAT] =
	{
		.gain				= ADC_GAIN_1,
		.reference			= ADC_REF_INTERNAL,
		.acquisition_time	= ADC_ACQ_TIME(ADC_ACQ_TIME_TICKS, 641),
		.channel_id	   		= 18,
		.differential		= 0,
	},
	[ADC_INPUT_TEMP] =
	{
		.gain				= ADC_GAIN_1,
		.reference			= ADC_REF_INTERNAL,
		.acquisition_time	= ADC_ACQ_TIME(ADC_ACQ_TIME_TICKS, 641),
		.channel_id	   		= 17,
		.differential		= 0,
	},
};

const struct adc_sequence sequence[] =
{
	[ADC_INPUT_VREF_INT] =
	{
		.channels	= BIT(0),
		.buffer	  = _adc_data,
		.buffer_size = sizeof(_adc_data),
		.resolution  = 12,
	},
	[ADC_INPUT_UNUSED] =
	{
		.channels	= BIT(1),
		.buffer	  = _adc_data,
		.buffer_size = sizeof(_adc_data),
		.resolution  = 12,
	},
	[ADC_INPUT_SENSOR_0] =
	{
		.channels	= BIT(2),
		.buffer	  = _adc_data,
		.buffer_size = sizeof(_adc_data),
		.resolution  = 12,
	},
	[ADC_INPUT_SENSOR_1] =
	{
		.channels	= BIT(3),
		.buffer	  = _adc_data,
		.buffer_size = sizeof(_adc_data),
		.resolution  = 12,
	},
	[ADC_INPUT_CHARGER] =
	{
		.channels	= BIT(4),
		.buffer	  = _adc_data,
		.buffer_size = sizeof(_adc_data),
		.resolution  = 12,
	},
	[ADC_INPUT_VBAT] =
	{
		.channels	= BIT(18),
		.buffer	  = _adc_data,
		.buffer_size = sizeof(_adc_data),
		.resolution  = 12,
	},
	[ADC_INPUT_TEMP] =
	{
		.channels	= BIT(17),
		.buffer	  = _adc_data,
		.buffer_size = sizeof(_adc_data),
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

static uint16_t _adc_read(struct device *dev, enum adc_input_e input)
{
	adc_read(dev, &sequence[input]);
	k_busy_wait(1000);
	return _adc_data[0];
}

static uint16_t _mes(enum adc_input_e input)
{
	uint16_t reference;
	uint16_t in;

	reference = _adc_read(adc_dev, ADC_INPUT_VREF_INT);
	in = _adc_read(adc_dev, input);

	LOG_DBG("ref:%"PRIu16" in: %"PRIu16, reference, in);
	return mes_mv(reference, in);
}

static uint16_t mes(enum adc_input_e input)
{
	const uint8_t mean_nr = 8;
	uint32_t tmp = 0;

	for (int i = 0 ; i < mean_nr ; i++)
	{
		tmp += _mes(input);
	}

	return tmp / mean_nr;
}

uint16_t adc_measure_charger(void)
{
	uint16_t value = mes(ADC_INPUT_CHARGER)*21;
	LOG_DBG("adc_measure_charger:%"PRIu16" mV", value);

	return value;
}

static uint16_t adc_measure_sensor0(void)
{
	uint16_t value = mes(ADC_INPUT_SENSOR_0);
	LOG_DBG("adc_measure_sensor0:%"PRIu16" mV", value);
	return value;
}

static uint16_t adc_measure_sensor1(void)
{
	uint16_t value = mes(ADC_INPUT_SENSOR_1);
	LOG_DBG("adc_measure_sensor1:%"PRIu16" mV", value);
	return value;
}

uint16_t adc_measure_vbat(void)
{
	uint16_t value;
	LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(), LL_ADC_PATH_INTERNAL_VREFINT | LL_ADC_PATH_INTERNAL_VBAT);
	value = mes(ADC_INPUT_VBAT)*3;
	LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(), LL_ADC_PATH_INTERNAL_VREFINT);
	LOG_DBG("adc_measure_vbat:%"PRIu16" mV", value);
	return value;
}

// Adapted from __LL_ADC_CALC_TEMPERATURE to have decidegrees
#define calc_temp_x10(__VREFANALOG_VOLTAGE__,\
                                  __TEMPSENSOR_ADC_DATA__,\
                                  __ADC_RESOLUTION__)                              \
  (((( ((int32_t)((__LL_ADC_CONVERT_DATA_RESOLUTION((__TEMPSENSOR_ADC_DATA__),     \
                                                    (__ADC_RESOLUTION__),          \
                                                    LL_ADC_RESOLUTION_12B)         \
                   * (__VREFANALOG_VOLTAGE__))                                     \
                  / TEMPSENSOR_CAL_VREFANALOG)                                     \
        - (int32_t) *TEMPSENSOR_CAL1_ADDR)                                         \
	) * (int32_t)(10*TEMPSENSOR_CAL2_TEMP - 10*TEMPSENSOR_CAL1_TEMP)                    \
    ) / (int32_t)((int32_t)*TEMPSENSOR_CAL2_ADDR - (int32_t)*TEMPSENSOR_CAL1_ADDR) \
) + 10*TEMPSENSOR_CAL1_TEMP                                                        \
)

int16_t adc_measure_temp(void)
{
	int32_t value;

	LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(), LL_ADC_PATH_INTERNAL_VREFINT | LL_ADC_PATH_INTERNAL_TEMPSENSOR);

	const uint8_t mean_nr = 8;
	int32_t reference = 0;
	int32_t in = 0;

	for (int i = 0 ; i < mean_nr ; i++)
	{
		reference += _adc_read(adc_dev, ADC_INPUT_VREF_INT);
		in += _adc_read(adc_dev, ADC_INPUT_TEMP);
	}

	reference /= mean_nr;
	in /= mean_nr;

	LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(), LL_ADC_PATH_INTERNAL_VREFINT);
	value = calc_temp_x10(vdda_mv(reference), in, LL_ADC_RESOLUTION_12B);
	LOG_DBG("adc_measure_temp:%"PRId32".%"PRId32" °C", value/10, value%10 > 0 ? value%10 : -value%10);
	return value;
}

uint16_t adc_measure_sensor(uint32_t sensor)
{
	switch (sensor)
	{
		case 0:
			return adc_measure_sensor0();
		break;
		case 1:
			return adc_measure_sensor1();
		break;
		default:
		return UINT16_MAX;
	}
}

void adc_init(void)
{
	adc_dev = device_get_binding(DT_ADC_1_NAME);
	stm32_setup_pins(pinconf, ARRAY_SIZE(pinconf));

	/*
	 * workaround : cpu board v0.1..v0.2
	 * prevent vbat flowing trough PC0, by tying PA8 to GND
	 *
	 * This will consume some uA if R100 is not removed
	 */
	struct device *pin = device_get_binding(DT_GPIO_STM32_GPIOA_LABEL);
	gpio_pin_configure(pin, 8, (GPIO_DIR_OUT));
	gpio_pin_write(pin, 8, 0);

	for (size_t i = 0; i < ARRAY_SIZE(channel_cfg); i++)
	{
		adc_channel_setup(adc_dev, &channel_cfg[i]);
	}

	uint16_t vdda = vdda_mv(_adc_read(adc_dev, ADC_INPUT_VREF_INT));
	LOG_INF("VREFINT_DATA=%"PRIu16", VDDA=%"PRIu16, *VREFINT_CAL_ADDR, vdda);
}

void adc_test(void)
{
	uint16_t vdda = vdda_mv(_adc_read(adc_dev, ADC_INPUT_VREF_INT));
	LOG_INF("VDDA=%"PRIu16"\n", vdda);
	LOG_INF("adc_measure_vbat():%"PRIu16" mV", adc_measure_vbat());
	LOG_INF("adc_measure_charger():%"PRIu16" mV", adc_measure_charger());
	LOG_INF("adc_measure_sensor0():%"PRIu16" mV", adc_measure_sensor0());
	LOG_INF("adc_measure_sensor1():%"PRIu16" mV", adc_measure_sensor1());
	int16_t value = adc_measure_temp();
	LOG_INF("adc_measure_temp():%"PRId16".%"PRId16" °C", value/10, value%10 > 0 ? value%10 : -value%10);
}
