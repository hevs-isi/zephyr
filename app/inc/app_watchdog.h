#ifndef APP_WATCHDOG_H
#define APP_WATCHDOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

int watchdog_init(uint32_t timeout_s);
void watchdog_feed(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_WATCHDOG_H */
