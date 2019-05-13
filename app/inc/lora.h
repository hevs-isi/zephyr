#ifndef _LORA_H
#define _LORA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void lora_init(void);
void lora_off(void);
void lora_on(void);
void disable_uart(void);
void enable_uart(void);
void lora_time_AppTimeReq(uint8_t AnsRequired);
void lora_send_info(void);

#ifdef __cplusplus
}
#endif

#endif /* _LORA_H */
