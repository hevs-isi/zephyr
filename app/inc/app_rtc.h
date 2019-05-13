#ifndef APP_RTC_H
#define APP_RTC_H

#ifdef __cplusplus
extern "C"
{
#endif

int app_rtc_init(void);
uint32_t app_rtc_get(void);
void app_rtc_set(uint32_t now);

#ifdef __cplusplus
}
#endif

#endif /* APP_RTC_H */
