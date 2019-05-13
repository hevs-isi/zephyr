#include <zephyr.h>
#include <misc/printk.h>
#include <counter.h>
#include <time.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(rtc, LOG_LEVEL_DBG);

#include "app_rtc.h"
#include "saved_config.h"
#include "global.h"

static struct device *rtc;

static char buf[40];

int app_rtc_init(void)
{
	rtc = device_get_binding(DT_RTC_0_NAME);

	time_t now = app_rtc_get();
	ctime_r(&now, buf);

	LOG_INF("Sytem date:%s", &buf[0]);

	return global.config.rtc_offset != 0;
}

uint32_t app_rtc_get(void)
{
	return counter_read(rtc) + global.config.rtc_offset;
}

void app_rtc_set(uint32_t now)
{
	uint32_t offset = now-counter_read(rtc);
	int32_t delta = offset - global.config.rtc_offset;

	if (delta < -60 || delta > 60 )
	{
		global.rtc_reset = 1;
	}

	global.config.rtc_offset = offset;
	saved_config_save(&global.config);
}
