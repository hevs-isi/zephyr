#ifndef _LORA_H
#define _LORA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * \brief Initialize the radio module
 *
 * \return 0 on success
 *
 * There is no need to call lora_on() before lora_init().
 */
int lora_init(void);

/**
 * \brief Turn the lora radio module OFF (including RX/TX pins)
 * \warning untested in production (can be impossible to turn back on)
 */
void lora_off(void);

/**
 * \brief Turn the lora radio module ON
 * \warning untested in production (can be impossible to turn back on)
 */
void lora_on(void);

/**
 * \brief Request the current time on the LoRaWAN network.
 *
 * \param AnsRequired 0 if no answer is necessary
 */
void lora_time_AppTimeReq(uint8_t AnsRequired);

/**
 * \brief Send general info the network
 */
void lora_send_info(void);

/**
 * \brief Converts network state to string
 *
 * \param network state
 *
 * \return a string describing the network state
 */
const char *nws2str(uint8_t state);


#ifdef __cplusplus
}
#endif

#endif /* _LORA_H */
