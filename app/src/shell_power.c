#include <zephyr.h>
#include <shell/shell.h>
#include <stdlib.h>
#include <device.h>
#include "stm32_lp.h"
#include "app_rtc.h"

#if CONFIG_SHELL

#include <logging/log.h>
LOG_MODULE_REGISTER(shell_power, LOG_LEVEL_INF);

static void shell_connected(const struct shell *shell)
{
	lp_sleep_prevent();
	shell_log_backend_enable(shell->log_backend, (void *)shell, LOG_LEVEL_DBG);
}

static int shell_sleep(const struct shell *shell, size_t argc, char *argv[])
{
	char *endptr;
	u32_t duration;

	if (argc != 2)
	{
		shell_error(shell, "arguments?");
		return -EINVAL;
	}
	duration = strtol(argv[1], &endptr, 0);

	if (endptr == argv[1])
	{
		shell_error(shell, "error parsing  argv[1] : '%s'", argv[1]);
		return -EINVAL;
	}

	k_sleep(1000);

	stm32_sleep(duration);

	return 0;
}

static int shell_on(const struct shell *shell, size_t argc, char *argv[])
{
	return 0;
}

static int shell_off(const struct shell *shell, size_t argc, char *argv[])
{
	return 0;
}

static int shell_rtc(const struct shell *shell, size_t argc, char *argv[])
{
	shell_connected(shell);

	char *endptr;
	u32_t now;

	if (argc == 1)
	{
		shell_print(shell, "rtc:%"PRIu32, app_rtc_get());
		return;
	}


	if (argc != 2)
	{
		shell_error(shell, "arguments?");
		return -EINVAL;
	}

	now = strtol(argv[1], &endptr, 0);

	if (endptr == argv[1])
	{
		shell_error(shell, "error parsing  argv[1] : '%s'", argv[1]);
		return -EINVAL;
	}

	int ret;

	shell_print(shell, "init rtc");

	LL_RTC_TimeTypeDef rtc_time;
	LL_RTC_DateTypeDef rtc_date;
	LL_RTC_TIME_StructInit(&rtc_time);
	LL_RTC_DATE_StructInit(&rtc_date);
	rtc_time.Hours		= 23U;
	rtc_time.Minutes	= 59U;
	rtc_time.Seconds	= 50U;
	rtc_date.WeekDay	= LL_RTC_WEEKDAY_MONDAY;
	rtc_date.Day    	= 28U;
	rtc_date.Month  	= 2U;
	rtc_date.Year   	= 19U;

	ret = LL_RTC_TIME_Init(RTC, LL_RTC_FORMAT_BIN, &rtc_time);
	if (ret)
	{
		shell_warn(shell, "LL_RTC_TIME_Init:%d", ret);
	}

	ret = LL_RTC_DATE_Init(RTC, LL_RTC_FORMAT_BIN, &rtc_date);
	if (ret)
	{
		shell_warn(shell, "LL_RTC_DATE_Init:%d", ret);
	}

	app_rtc_set(now);

	shell_print(shell, "rtc:%"PRIu32, app_rtc_get());

	return 0;
}

static int shell_swd(const struct shell *shell, size_t argc, char *argv[])
{
	shell_connected(shell);

	int enable;

	if (argc <= 1)
	{
		return -1;
	}

	if (argv[1][0] == '0')
	{
		enable = 0;
	}
	else if (argv[1][0] == '1')
	{
		enable = 1;
	}
	else
	{
		shell_error(shell, "unknown arg:'%s'", argv[1]);
		return -1;
	}

	if (enable)
	{
		shell_print(shell, "swd enable");
		stm32_swd_on();
	}
	else
	{
		shell_print(shell, "swd disable");
		stm32_swd_off();
	}

	return 0;
}

static int shell_quit(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	lp_sleep_permit();

	return 0;
}

SHELL_CREATE_STATIC_SUBCMD_SET(power_sub)
{
	SHELL_CMD_ARG(sleep, NULL, "sleep [ms]", shell_sleep, 0, 0),
	SHELL_CMD_ARG(rtc, NULL, "setup rtc date/time", shell_rtc, 0, 1),
	SHELL_CMD_ARG(swd, NULL, "[0/1]disable or enable swd pins", shell_swd, 2, 0),
	SHELL_CMD_ARG(quit, NULL, "re-enable power save mode", shell_quit, 0, 0),

	SHELL_CMD_ARG(off, NULL, "no help", shell_off, 0, 0),
	SHELL_CMD_ARG(on, NULL, "no help", shell_on, 0, 0),

	SHELL_SUBCMD_SET_END
};

SHELL_CMD_REGISTER(power, &power_sub, "power commands", NULL);

#endif /* CONFIG_SHELL */
