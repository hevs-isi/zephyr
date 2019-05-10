#ifndef APP_ADC_H
#define APP_ADC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void adc_init(void);
void adc_test(void);

uint16_t adc_measure_vbat(void);
uint16_t adc_measure_charger(void);
uint16_t adc_measure_sensor(uint32_t sensor);

#ifdef __cplusplus
}
#endif

#endif /* APP_ADC_H */
