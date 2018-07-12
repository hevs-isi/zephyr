/*
 * Copyright (c) 2018 Darko Petrovic
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <init.h>
#include <board.h>
#include <gpio.h>
#include <misc/printk.h>

#include <string.h>
#include <i2c_switch.h>
#include <logging/sys_log.h>

struct sensor_pwr_ctrl_cfg {
	const char *name;
	const char *port;
	u32_t pin;
	u8_t polarity;
	s8_t i2c_channel;
};

static const struct sensor_pwr_ctrl_cfg pwr_ctrl_cfg[] = {
#if CONFIG_SHTC1
	{	.name = CONFIG_SHTC1_NAME,
		.port = SHT_POWER_PORT,
		.pin = SHT_POWER_PIN,
		.polarity = 1,
		.i2c_channel = SHT_I2C_CHANNEL
	},
#endif
#if CONFIG_OPT3002
	{
		.name = CONFIG_OPT3002_NAME,
		.port = OPT_POWER_PORT,
		.pin = OPT_POWER_PIN,
		.polarity = 1,
		.i2c_channel = OPT_I2C_CHANNEL
	},
#endif
#if CONFIG_CCS811
	{
		.name = CONFIG_CCS811_NAME,
		.port = CCS_POWER_PORT,
		.pin = CCS_POWER_PIN,
		.polarity = 0,
		.i2c_channel = CCS_I2C_CHANNEL
	},
#endif
#if CONFIG_PYD1588
	{
		.name = CONFIG_PYD1588_NAME,
		.port = PYD_POWER_PORT,
		.pin = PYD_POWER_PIN,
		.polarity = 1,
		.i2c_channel = -1
	},
#endif
	{
		.name = CONFIG_ANALOGMIC_NAME,
		.port = MIC_POWER_PORT,
		.pin = MIC_POWER_PIN,
		.polarity = 1,
		.i2c_channel = -1
	},
	{
		.name = "PAC1934",
		.port = PAC1934_POWER_PORT,
		.pin = PAC1934_POWER_PIN,
		.polarity = 1,
		.i2c_channel = -1
	}
};

static int pwr_ctrl_init(struct device *dev)
{
	u8_t i;
	const struct sensor_pwr_ctrl_cfg *cfg = dev->config->config_info;
	struct device *gpio;

	// enable i2c switch by setting the n_reset pin
	gpio = device_get_binding(MUX_RESET_PORT);
	gpio_pin_configure(gpio, MUX_RESET_PIN, GPIO_DIR_OUT);
	gpio_pin_write(gpio, MUX_RESET_PIN, 1);

	// turn off all sensors by default
	for(i=0;i<sizeof(pwr_ctrl_cfg)/sizeof(pwr_ctrl_cfg[0]);i++){
		gpio = device_get_binding(cfg[i].port);
		if (!gpio) {
			printk("Could not bind device \"%s\"\n", cfg[i].port);
			return -ENODEV;
		}

		gpio_pin_configure(gpio, cfg[i].pin, GPIO_DIR_OUT);
		gpio_pin_write(gpio, cfg[i].pin, !cfg[i].polarity);
	}

	//gpio = device_get_binding(MIC_OUTPUT_PORT);
	//gpio_pin_configure(gpio, MIC_OUTPUT_PIN, GPIO_DIR_IN);

	return 0;
}

void power_sensor(const char *name, u8_t power){
	struct device *gpio;
	struct device *dev;
	u8_t i;
	u8_t buf[1];

	// power on/off the sensor and de/activate the corresponding I2C channel
	for(i=0;i<sizeof(pwr_ctrl_cfg)/sizeof(pwr_ctrl_cfg[0]);i++){
		if(strcmp(name, pwr_ctrl_cfg[i].name) == 0){
			gpio = device_get_binding(pwr_ctrl_cfg[i].port);
			gpio_pin_write(gpio, pwr_ctrl_cfg[i].pin, !(power ^ pwr_ctrl_cfg[i].polarity));

			// TODO_ need to be adjusted
			k_busy_wait(100);
#if CONFIG_I2C_SWITCH
			if(pwr_ctrl_cfg[i].i2c_channel >= 0){
				SYS_LOG_DBG("Configure i2c channel for %s.", name);
				dev = device_get_binding(CONFIG_I2C_SWITCH_I2C_MASTER_DEV_NAME);
				// read i2c switch status
				if(i2c_read(dev, buf, 1, CONFIG_I2C_SWITCH_ADDR) > 0){
					SYS_LOG_DBG("Failed to read data sample!");
				}

				if(power){
					buf[0] |= (1 << pwr_ctrl_cfg[i].i2c_channel);
				} else {
					buf[0] &= ~(1 << pwr_ctrl_cfg[i].i2c_channel);
				}

				//buf[0] = (1 << pwr_ctrl_cfg[i].i2c_channel);

				i2c_write(dev, buf, 1, CONFIG_I2C_SWITCH_ADDR);
			}
#endif

			if(power){
				dev = device_get_binding(pwr_ctrl_cfg[i].name);
				if(dev){
					dev->config->init(dev);
					SYS_LOG_DBG("Sensor init");
				}
			}


			break;
		}
	}

}


DEVICE_INIT(vdd_pwr_ctrl, "SENSOR_PWR_CTRL", pwr_ctrl_init, NULL, pwr_ctrl_cfg,
	    POST_KERNEL, 50);

