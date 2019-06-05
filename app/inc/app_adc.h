#ifndef APP_ADC_H
#define APP_ADC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * \brief Initialize the ADC and related pins
 */
void adc_init(void);

/**
 * \brief measure all (connected) ADC inputs and print results to the logger.
 */
void adc_test(void);

/**
 * \brief measure the battery voltage
 * \return the value in mV.
 */
uint16_t adc_measure_vbat(void);

/**
 * \brief measure the battery charger (solar panel)
 * \return the value in mV.
 */
uint16_t adc_measure_charger(void);

/**
 * \brief measure the analog inputs
 * \param sensor, 0 for sensor 0, 1 for sensor 1
 * \return the value in mV.
 */
uint16_t adc_measure_sensor(uint32_t sensor);

#ifdef __cplusplus
}
#endif

#endif /* APP_ADC_H */
