#include <zephyr.h>

#include <soc_registers.h>
#include <soc.h>
#include <power.h>
#include <counter.h>
#include <logging/log.h>
#include <pinmux/stm32/pinmux_stm32.h>
#include <gpio.h>
#include "stm32_lp.h"
#include "global.h"
#include "buttons.h"

LOG_MODULE_REGISTER(lp, LOG_LEVEL_INF);

#define ALARM_CHANNEL_ID 0

static void counter_isr(struct device *counter_dev, u8_t chan_id,
	u32_t ticks, void *user_data)
{
	LOG_DBG("here");
}

void stm32_sleep(u32_t duration)
{
	struct device *counter_dev;
	int ret;
	struct counter_alarm_cfg alarm_cfg;

	counter_dev = device_get_binding(DT_RTC_0_NAME);
	LOG_DBG("stm32_sleep(%d)", duration);
	LOG_DBG("counter_get_pending_int:%d", counter_get_pending_int(counter_dev));

	//counter_start(counter_dev);

	alarm_cfg.absolute = false;
	alarm_cfg.ticks = counter_us_to_ticks(counter_dev, duration*1000);
	if (alarm_cfg.ticks < 2) {
		alarm_cfg.ticks = 2;
	}
	alarm_cfg.callback = counter_isr;
	alarm_cfg.user_data = &alarm_cfg;

	LOG_DBG("alarm_cfg.ticks=%d", alarm_cfg.ticks);

	ret = counter_set_channel_alarm(counter_dev, ALARM_CHANNEL_ID, &alarm_cfg);
	if (ret)
	{
		LOG_ERR("counter_set_channel_alarm:%d", ret);
		return ;
	}

	/*
	EXTI->PR1 = 0xffffffff;
	EXTI->PR2 = 0xffffffff;

	LL_RTC_DisableWriteProtection(RTC);
	//RTC->SCR = -1;
	LL_RTC_ClearFlag_TAMP2(RTC);
	LL_RTC_ClearFlag_TAMP1(RTC);
	LL_RTC_ClearFlag_TSOV(RTC);
	LL_RTC_ClearFlag_TS(RTC);
	LL_RTC_ClearFlag_WUT(RTC);
	LL_RTC_ClearFlag_ALRA(RTC);
	LL_RTC_ClearFlag_ALRB(RTC);
	LL_RTC_ClearFlag_RS(RTC);
	LL_RTC_EnableWriteProtection(RTC);

	*/
	//RTC->
	//HAL_PWR_EnterSTOPMode(HAL_PWR_EnterSLEEPMode, PWR_STOPENTRY_WFI);
	//HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
	uart_wakeup_enable();
	HAL_SuspendTick();
	//sys_clock_disable();
	LL_RTC_DisableWriteProtection(RTC);
	LL_RTC_ClearFlag_ALRA(RTC);
	//LL_RTC_SetAlarmOutEvent(RTC, LL_RTC_ALARMOUT_ALMA);
	LL_RTC_EnableWriteProtection(RTC);
	LL_RCC_SetClkAfterWakeFromStop(LL_RCC_STOP_WAKEUPCLOCK_MSI);
	HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);

	/*
	HAL_PWREx_EnableGPIOPullDown(PWR_GPIO_C, 13);
	HAL_PWREx_EnableGPIOPullDown(PWR_GPIO_D, 2);
	HAL_PWREx_EnablePullUpPullDownConfig();
	HAL_PWR_EnterSTANDBYMode();
	*/
	//HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE2);
	//HAL_PWREx_DisableLowPowerRunMode();
	//z_clock_idle_exit();
	uart_wakeup_disable();
	HAL_ResumeTick();
	k_cycle_get_32();
	//printk("toto\n");

	//LOG_DBG("counter_get_pending_int:%d", counter_get_pending_int(counter_dev));
	//LOG_DBG("EXTI->PR1:0x%"PRIx32, EXTI->PR1);
	//LOG_DBG("EXTI->PR2:0x%"PRIx32, EXTI->PR2);

	ret = counter_cancel_channel_alarm(counter_dev, ALARM_CHANNEL_ID);
	if (ret)
	{
		LOG_ERR("counter_cancel_channel_alarm:%d", ret);
		return ;
	}

	//HAL_PWR_EnterSLEEPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
	//HAL_PWREx_DisableLowPowerRunMode();
}

#define INPUT_NOPULL (STM32_MODER_INPUT_MODE | STM32_PUPDR_NO_PULL)
#define INPUT_PULL_DOWN (STM32_MODER_INPUT_MODE | STM32_PUPDR_PULL_DOWN)
#define INPUT_PULL_UP (STM32_MODER_INPUT_MODE | STM32_PUPDR_PULL_UP)

static const struct pin_config pinconf[] = {
	{STM32_PIN_PA0 , INPUT_PULL_DOWN},
	{STM32_PIN_PA1 , INPUT_PULL_DOWN},
	{STM32_PIN_PA2 , (STM32_PINMUX_ALT_FUNC_7 | STM32_PUPDR_NO_PULL)},
	{STM32_PIN_PA3 , (STM32_PINMUX_ALT_FUNC_7 | STM32_PUPDR_PULL_UP)},	// Pull up because the debugger is not connected in real use
	{STM32_PIN_PA4 , INPUT_PULL_DOWN},
	{STM32_PIN_PA5 , INPUT_PULL_DOWN},
	{STM32_PIN_PA6 , INPUT_PULL_DOWN},
	{STM32_PIN_PA7 , INPUT_PULL_DOWN},
	{STM32_PIN_PA8 , INPUT_NOPULL},
	{STM32_PIN_PA9 , INPUT_PULL_DOWN},
	{STM32_PIN_PA10, INPUT_PULL_DOWN},
	{STM32_PIN_PA11, INPUT_PULL_DOWN},
	{STM32_PIN_PA12, INPUT_PULL_DOWN},
	{STM32_PIN_PA13, STM32_PINMUX_ALT_FUNC_0 | STM32_PUPDR_PULL_UP}, //SWDIO
	{STM32_PIN_PA14, STM32_PINMUX_ALT_FUNC_0 | STM32_PUPDR_PULL_DOWN}, //SWDCLK
	{STM32_PIN_PA15, INPUT_PULL_DOWN},

	{STM32_PIN_PB0 , INPUT_PULL_DOWN},
	{STM32_PIN_PB1 , INPUT_PULL_DOWN},
	{STM32_PIN_PB2 , INPUT_PULL_DOWN},
	{STM32_PIN_PB3 , INPUT_PULL_DOWN},
	{STM32_PIN_PB4 , INPUT_PULL_DOWN},
	{STM32_PIN_PB5 , INPUT_NOPULL},
	{STM32_PIN_PB6 , INPUT_NOPULL},
	{STM32_PIN_PB7 , INPUT_PULL_DOWN},
	{STM32_PIN_PB8 , INPUT_PULL_DOWN},
	{STM32_PIN_PB9 , INPUT_PULL_DOWN},
	{STM32_PIN_PB10, INPUT_PULL_UP},
	{STM32_PIN_PB11, INPUT_PULL_UP},
	{STM32_PIN_PB12, INPUT_PULL_UP},
	{STM32_PIN_PB13, INPUT_PULL_DOWN},
	{STM32_PIN_PB14, INPUT_PULL_DOWN},
	{STM32_PIN_PB15, INPUT_PULL_UP},

	{STM32_PIN_PC0 , INPUT_NOPULL},
	{STM32_PIN_PC1 , INPUT_NOPULL},
	{STM32_PIN_PC2 , INPUT_NOPULL},
	{STM32_PIN_PC3 , INPUT_NOPULL},
	{STM32_PIN_PC4 , INPUT_PULL_DOWN},
	{STM32_PIN_PC5 , INPUT_PULL_DOWN},
	{STM32_PIN_PC6 , INPUT_PULL_DOWN},
	{STM32_PIN_PC7 , INPUT_PULL_DOWN},
	{STM32_PIN_PC8 , INPUT_NOPULL},
	{STM32_PIN_PC9 , INPUT_NOPULL},
	{STM32_PIN_PC10, INPUT_PULL_DOWN},
	{STM32_PIN_PC11, INPUT_PULL_UP},
	{STM32_PIN_PC12, INPUT_PULL_DOWN},
	{STM32_PIN_PC13, INPUT_PULL_DOWN},
#if !CONFIG_COUNTER_RTC_STM32_CLOCK_LSE
	{STM32_PIN_PC14, INPUT_PULL_DOWN},
	{STM32_PIN_PC15, INPUT_PULL_DOWN},
#endif
	{STM32_PIN_PD2 , INPUT_NOPULL},
	{STM32_PIN_PH0 , INPUT_PULL_DOWN},
	{STM32_PIN_PH1 , INPUT_PULL_DOWN},
	{STM32_PIN_PH3 , INPUT_PULL_DOWN},
};

static const struct pin_config pinconf_swd_off[] = {
	{STM32_PIN_PA13, INPUT_PULL_UP},
	{STM32_PIN_PA14, INPUT_PULL_DOWN},
};

static const struct pin_config pinconf_swd_on[] = {
	{STM32_PIN_PA13, STM32_PINMUX_ALT_FUNC_0 | STM32_PUPDR_PULL_UP}, //SWDIO
	{STM32_PIN_PA14, STM32_PINMUX_ALT_FUNC_0 | STM32_PUPDR_PULL_DOWN}, //SWDCLK
};

void stm32_swd_off(void)
{
	stm32_setup_pins(pinconf_swd_off, ARRAY_SIZE(pinconf_swd_off));
}

void stm32_swd_on(void)
{
	stm32_setup_pins(pinconf_swd_on, ARRAY_SIZE(pinconf_swd_on));
}

void lp_init(void)
{
	stm32_setup_pins(pinconf, ARRAY_SIZE(pinconf));

	HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE2);

	struct device *counter_dev;

	counter_dev = device_get_binding(DT_RTC_0_NAME);
	counter_start(counter_dev);
}

enum power_states sys_suspend(s32_t ticks)
{
	#ifdef CONFIG_TRACING
		z_sys_trace_idle(ticks);
	#endif /* CONFIG_TRACING */

	/* clear BASEPRI so wfi is awakened by incoming interrupts */
	__asm__("eors.n r0, r0\nmsr BASEPRI, r0\n");

	HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFI);

	return SYS_POWER_STATE_AUTO;
}

void psu_5v(u32_t enable)
{
	int ret;
	struct device *dev;

	/* Power off gps backup */
	dev = device_get_binding(DT_GPIO_STM32_GPIOB_LABEL);
	ret = gpio_pin_configure(dev, 8, (GPIO_DIR_OUT));
	ret = gpio_pin_write(dev, 8, enable);
}

void psu_ind(u32_t enable)
{
	int ret;
	struct device *dev;

	/* Power off gps backup */
	dev = device_get_binding(DT_GPIO_STM32_GPIOB_LABEL);
	ret = gpio_pin_configure(dev, 14, (GPIO_DIR_OUT));
	ret = gpio_pin_write(dev, 14, enable);
}

static const struct pin_config pinconf_charge_on[] = {
	{STM32_PIN_PB13, INPUT_NOPULL},
};

void psu_charge(u32_t enable)
{
	int ret;
	struct device *dev;

	if (enable)
	{
		stm32_setup_pins(pinconf_charge_on, ARRAY_SIZE(pinconf_charge_on));
	}
	else
	{
		dev = device_get_binding(DT_GPIO_STM32_GPIOB_LABEL);
		ret = gpio_pin_configure(dev, 13, (GPIO_DIR_OUT));
		ret = gpio_pin_write(dev, 13, 0);
	}
}

void psu_cpu_hp(u32_t enable)
{
	int ret;
	struct device *dev;

	/* Power off gps backup */
	dev = device_get_binding(DT_GPIO_STM32_GPIOA_LABEL);
	ret = gpio_pin_configure(dev, 11, (GPIO_DIR_OUT));
	ret = gpio_pin_write(dev, 11, enable);
}

void sys_resume(void)
{

}

void lp_sleep_prevent(void)
{
	global.sleep_prevent = 1;
}

void lp_sleep_permit(void)
{
	global.sleep_permit = 1;
}
