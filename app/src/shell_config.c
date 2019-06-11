#include <zephyr.h>
#include <shell/shell.h>
#include <stdlib.h>
#include <device.h>
#include <counter.h>
#include "stm32_lp.h"
#include "saved_config.h"
#include "global.h"

#if CONFIG_SHELL

#include <logging/log.h>
LOG_MODULE_REGISTER(shellconfig, LOG_LEVEL_DBG);

static void shell_connected(const struct shell *shell)
{
	lp_sleep_prevent();
	shell_log_backend_enable(shell->log_backend, (void *)shell, LOG_LEVEL_DBG);
}

static int shell_set_off(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_connected(shell);

	saved_config_predef(&global.config, SC_DEFAULT_OFF);
	global.config_changed = 1;

	return 0;
}

static int shell_set_indus_1(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_connected(shell);

	saved_config_predef(&global.config, SC_DEFAULT_INDUS_1);
	global.config_changed = 1;

	return 0;
}

static int shell_set_indus_2(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_connected(shell);

	saved_config_predef(&global.config, SC_DEFAULT_INDUS_2);
	global.config_changed = 1;

	return 0;
}

static int shell_set_indus_1_fast(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_connected(shell);

	saved_config_predef(&global.config, SC_DEFAULT_INDUS_1_FAST);
	global.config_changed = 1;

	return 0;
}

static int shell_set_indus_2_fast(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_connected(shell);

	saved_config_predef(&global.config, SC_DEFAULT_INDUS_2_FAST);
	global.config_changed = 1;

	return 0;
}

static int config_print(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_connected(shell);

	for (size_t i = 0 ; i < ARRAY_SIZE(global.config.sensor_config); i++)
	{
		struct sensor_config_t *c = &global.config.sensor_config[i];
		shell_print(shell, "config[%zu].enable:%"PRIu8, i, c->enable);
		if (c->enable)
		{
			shell_print(shell, "config[%zu].period:%"PRIu32" s", i, c->period);
			shell_print(shell, "config[%zu].wakeup_ms:%"PRIu32, i, c->wakeup_ms);
		}
	}

	return 0;
}

SHELL_CREATE_STATIC_SUBCMD_SET(config_sub)
{
	SHELL_CMD_ARG(set_off, NULL, "sleep [ms]", shell_set_off, 0, 0),
	SHELL_CMD_ARG(set_indus_1, NULL, "sleep [ms]", shell_set_indus_1, 0, 0),
	SHELL_CMD_ARG(set_indus_2, NULL, "sleep [ms]", shell_set_indus_2, 0, 0),
	SHELL_CMD_ARG(set_indus_1_fast, NULL, "sleep [ms]", shell_set_indus_1_fast, 0, 0),
	SHELL_CMD_ARG(set_indus_2_fast, NULL, "sleep [ms]", shell_set_indus_2_fast, 0, 0),
	SHELL_CMD_ARG(print, NULL, "display current config", config_print, 0, 0),

	SHELL_SUBCMD_SET_END
};

SHELL_CMD_REGISTER(config, &config_sub, "power commands", NULL);

#endif /* CONFIG_SHELL */
