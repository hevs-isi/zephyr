#ifndef APP_RTC_H
#define APP_RTC_H

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * \brief Initialize the RTC
 */
int app_rtc_init(void);

/**
 * \brief Get the RTC time (time since unix epoch, UTC)
 */
uint32_t app_rtc_get(void);

/**
 * \brief Set the RTC time (time since unix epoch, UTC)
 */
void app_rtc_set(uint32_t now);

/**
 * \brief Guess if the RTC has been initialized (or a power lost has occured)
 */
int app_rtc_ok(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_RTC_H */
