#include <adc.h>
#include <zephyr.h>
#include <misc/printk.h>
#include <soc.h>
#include <gpio.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(watchdog, LOG_LEVEL_INF);

#include "app_watchdog.h"
#include <watchdog.h>

static int wdt_channel_id;
static struct device *wdt;

int watchdog_init(uint32_t timeout_s)
{
	int err;
	struct wdt_timeout_cfg wdt_config;

	wdt = device_get_binding(DT_WDT_0_NAME);

	if (!wdt) {
		LOG_ERR("watchdog not found");
		return -ENODEV;
	}

	/* Reset SoC when watchdog timer expires. */
	wdt_config.flags = WDT_FLAG_RESET_SOC;

	wdt_config.window.min = 0U;
	wdt_config.window.max = timeout_s * 1000;

	wdt_config.callback = NULL;

	wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
	if (wdt_channel_id < 0) {
		LOG_ERR("watchdog install error");
		return -ENODEV;
	}

	err = wdt_setup(wdt, 0);
	if (err < 0) {
		LOG_ERR("watchdog setup error");
		return -ENODEV;
	}

	return 0;
}

void watchdog_feed(void)
{
	wdt_feed(wdt, wdt_channel_id);
}
