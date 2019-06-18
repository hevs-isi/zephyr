#include <zephyr.h>
#include <shell/shell.h>
#include <stdlib.h>

#if CONFIG_SHELL

#include <logging/log.h>
LOG_MODULE_REGISTER(shell_lora, LOG_LEVEL_DBG);

#include <shell/shell_log_backend.h>
#include <shell/shell_uart.h>

#ifdef CONFIG_LORA_IM881A
#include <wimod_lorawan_api.h>
#endif
#include "lora.h"
#include "shell_lora.h"
#include "stm32_lp.h"

void lora_shell_pm(void)
{
	/* FIXME : crashes without the sleep call */
	k_sleep(1);
	shell_execute_cmd(shell_backend_uart_get_ptr(), "lora quit");
}

static void shell_connected(const struct shell *shell)
{
	lp_sleep_prevent();
	shell_log_backend_enable(shell->log_backend, (void *)shell, LOG_LEVEL_DBG);
}

static int shell_cmd_info(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	shell_connected(shell);

	struct lw_dev_info_t info;

	int status = wimod_lorawan_get_device_info(&info);

	if (status)
	{
		shell_error(shell, "failed");
		return 0;
	}

	shell_print(shell, "type:0x%02"PRIx8, info.type);
	shell_print(shell, "addr:0x%08"PRIx32, info.addr);
	shell_print(shell, "id:0x%08"PRIx32, info.id);

	return 0;
}

static int shell_cmd_fw(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	shell_connected(shell);

	struct lw_firmware_info_t info;

	int status = wimod_lorawan_get_firmware_version(&info);

	if (status)
	{
		shell_error(shell, "failed");
		return 0;
	}

	shell_print(shell, "version:%"PRIu8".%"PRIu8, info.maj, info.min);
	shell_print(shell, "build:%"PRIu16, info.build);
	shell_print(shell, "date:%s", info.date_str);
	if (info.info_str[0])
	{
		shell_print(shell, "info:%s", info.info_str);
	}

	return 0;
}

static const char *nws2str(uint8_t state)
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

static int shell_nwk_status(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	shell_connected(shell);

	struct lw_net_status_t nws;

	int status = wimod_lorawan_get_nwk_status(&nws);
	if (status)
	{
		shell_error(shell, "failed");
		return 0;
	}

	shell_print(shell, "state: 0x%02"PRIu8" (%s)", nws.state, nws2str(nws.state));
	if (nws.state == 0x01 || nws.state == 0x02)
	{
		shell_print(shell, "address: 0x%06"PRIx32, nws.addr);
		shell_print(shell, "dr: %"PRIu8, nws.dr);
		shell_print(shell, "power: %"PRIu8, nws.power);
		shell_print(shell, "max_payload: %"PRIu8, nws.max_payload);
	}

	return 0;
}


static int shell_cmd_deveui(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	shell_connected(shell);

	struct lw_dev_eui_t eui;

	int status = wimod_lorawan_get_device_eui(&eui);
	if (status)
	{
		shell_error(shell, "failed");
		return 0;
	}

	shell_print(shell, "eui: 0x%016"PRIx64, eui.eui);

	return 0;
}

static int shell_cmd_custom(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	shell_connected(shell);

	wimod_lorawan_set_op_mode();

	return 0;
}

static int shell_cmd_getmode(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	shell_connected(shell);

	wimod_lorawan_get_op_mode();

	return 0;
}

static int shell_cmd_get_rtc(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	shell_connected(shell);

	wimod_lorawan_get_rtc();

	return 0;
}

static int shell_cmd_set_rtc(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	shell_connected(shell);

	wimod_lorawan_set_rtc();

	return 0;
}

static int shell_cmd_get_rtc_alarm(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	shell_connected(shell);

	wimod_lorawan_get_rtc_alarm();

	return 0;
}

static int shell_factory_reset(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	shell_connected(shell);

	wimod_lorawan_factory_reset();

	return 0;
}

static int shell_cmd_setparam(const struct shell *shell, size_t argc, char *argv[])
{
	shell_connected(shell);

	if (argc != 3)
	{
		return -1;
	}

	const char *appEui = argv[1]; //"70B3D57ED0017ED9";
	const char *appKey = argv[2]; //"3F72D483526535F171F1CBA50FDDB884";

	// lora set 70B3D57ED0017ED9 3F72D483526535F171F1CBA50FDDB884

	if (strlen(appEui) != 16)
	{
		shell_error(shell, "Invalid APPEUI, length MUST be 16 (was '%s')\n", appEui);
		return -1;
	}

	if (strlen(appKey) != 32)
	{
		shell_error(shell, "Invalid APPKEY, length MUST be 32 (was '%s')\n", appKey);
		return -1;
	}

	/*
	 * Some modules (at least im881) may come from the factory as class C device
	 * So configure the module as class A.
	 */
	wimod_lorawan_set_rstack_config();

	wimod_lorawan_set_join_param_request(appEui, appKey);

	return 0;
}

static int shell_cmd_join(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	shell_connected(shell);

	wimod_lorawan_join_network_request(NULL);

	return 0;
}

static int shell_set_rstack_cfg(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	shell_connected(shell);

	wimod_lorawan_set_rstack_config();

	return 0;
}

static int shell_rstack_cfg(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	shell_connected(shell);

	wimod_lorawan_get_rstack_config();

	return 0;
}

static int shell_send_data(const struct shell *shell, uint32_t confirmed)
{
	u8_t data[3];
	struct lw_tx_result_t txr;
	int result;
	shell_connected(shell);

	data[0] = 0x11;
	data[1] = 0x22;
	data[2] = 0x33;

	if (confirmed)
	{
		result = wimod_lorawan_send_c_radio_data(1, data, 3, &txr);
	}
	else
	{
		result = wimod_lorawan_send_u_radio_data(1, data, 3, &txr);
	}

	if (result)
	{
		shell_error(shell, "failed");
		return 0;
	}

	if (txr.status)
	{
		shell_error(shell, "failed, channel blocked for %"PRIu32" ms", txr.ms_delay);
	}

	return 0;
}

static int shell_send_udata(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return shell_send_data(shell, 0);
}

static int shell_send_cdata(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return shell_send_data(shell, 1);
}

#include <device.h>
#include <gpio.h>

static int shell_quit(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_log_backend_disable(shell->log_backend);

	shell_print(shell, "logging disabled until next command");

	int ret;

	struct device *dev = device_get_binding(DT_GPIO_STM32_GPIOA_LABEL);
	ret = gpio_pin_configure(dev, 0, (GPIO_DIR_IN));
	if (ret) {
		shell_error(shell, "Error configuring pin PA%d!\n", 13);
		return ret;
	}

	return 0;
}

static int shell_cmd_tx_info(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	shell_connected(shell);

	lora_send_info();

	return 0;
}

static int shell_time(const struct shell *shell, size_t argc, char *argv[])
{
	shell_connected(shell);
	int required;

	if (argc <= 1)
	{
		return -1;
	}

	if (argv[1][0] == '0')
	{
		required = 0;
	}
	else if (argv[1][0] == '1')
	{
		required = 1;
	}
	else
	{
		shell_error(shell, "unknown arg:'%s'", argv[1]);
		return -1;
	}

	lora_time_AppTimeReq(required);

	return 0;
}

static int shell_reset(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_connected(shell);

	return wimod_lorawan_reset();
}

SHELL_STATIC_SUBCMD_SET_CREATE(lora_sub,
	SHELL_CMD_ARG(set, NULL, "set APPEUI APPKEY", shell_cmd_setparam, 2, 2),
	SHELL_CMD_ARG(join, NULL, "no help", shell_cmd_join, 0, 1),
	SHELL_CMD_ARG(deveui, NULL, "read deveui", shell_cmd_deveui, 0, 1),
	SHELL_CMD_ARG(tx_info, NULL, "tx lora_info", shell_cmd_tx_info, 0, 0),
	SHELL_CMD_ARG(factory, NULL, "reset to factory settings (will erase network keys)", shell_factory_reset, 0, 0),
	SHELL_CMD_ARG(time, NULL, "time [0/1], will request server time (1 for mandatory response)", shell_time, 1, 1),
	SHELL_CMD_ARG(reset, NULL, "reset the lora module", shell_reset, 0, 0),

	SHELL_CMD_ARG(info, NULL, "no help", shell_cmd_info, 0, 1),
	SHELL_CMD_ARG(firmware, NULL, "no help", shell_cmd_fw, 0, 1),
	SHELL_CMD_ARG(custom, NULL, "no help", shell_cmd_custom, 0, 1),
	SHELL_CMD_ARG(getmode, NULL, "no help", shell_cmd_getmode, 0, 1),
	SHELL_CMD_ARG(get_rtc, NULL, "no help", shell_cmd_get_rtc, 0, 1),
	SHELL_CMD_ARG(set_rtc, NULL, "no help", shell_cmd_set_rtc, 0, 1),
	SHELL_CMD_ARG(get_rtc_alarm, NULL, "no help", shell_cmd_get_rtc_alarm, 0, 1),
	SHELL_CMD_ARG(nwk, NULL, "no help", shell_nwk_status, 0, 1),
	SHELL_CMD_ARG(setrstack, NULL, "no help", shell_set_rstack_cfg, 0, 1),
	SHELL_CMD_ARG(rstack, NULL, "no help", shell_rstack_cfg, 0, 1),
	SHELL_CMD_ARG(set_rstack, NULL, "no help", shell_set_rstack_cfg, 0, 1),
	SHELL_CMD_ARG(udata, NULL, "no help", shell_send_udata, 0, 1),
	SHELL_CMD_ARG(cdata, NULL, "no help", shell_send_cdata, 0, 1),
	SHELL_CMD_ARG(quit, NULL, "no help", shell_quit, 0, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(lora, &lora_sub, "LoRa commands", NULL);

#endif /* CONFIG_SHELL */
