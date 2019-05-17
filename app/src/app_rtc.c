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
	int ok;
	rtc = device_get_binding(DT_RTC_0_NAME);

	ok = app_rtc_ok();

	time_t now = app_rtc_get();
	ctime_r(&now, buf);

	if (ok)
	{
		LOG_INF("Sytem date:%s", &buf[0]);
	}
	else
	{
		LOG_ERR("Wrong sytem date:%s", &buf[0]);
	}

	return !ok;
}

int app_rtc_ok(void)
{
	time_t now = app_rtc_get();
	struct tm tm;
	gmtime_r(&now, &tm);

	if (tm.tm_year < 2019)
	{
		return 0;
	}

	return 1;
}

uint32_t app_rtc_get(void)
{
	return counter_read(rtc) + global.config.rtc_offset;
}

void app_rtc_set(uint32_t now)
{
	uint32_t offset = now-counter_read(rtc);
	uint32_t delta = offset - global.config.rtc_offset;
	LOG_DBG("now = %"PRIu32, now);
	LOG_DBG("offset = %"PRId32, offset);
	LOG_INF("delta = %"PRId32, delta);

	if ((delta > 60) && (delta < UINT32_MAX - 60))
	{
		LOG_INF("delta > 60 !!!!");
		global.rtc_reset = 1;
	}

	global.config.rtc_offset = offset;
	saved_config_save(&global.config);
}
