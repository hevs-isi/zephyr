#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * \brief Initialize the buttons
 *
 * button1 will trigger a measure and will send result trought the radio
 * buttio2 will enable the console for 60 seconds.
 */
void buttons_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BUTTONS_H */
