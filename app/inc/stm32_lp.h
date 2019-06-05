#ifndef STM32_LP_H
#define STM32_LP_H

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * \brief Sleep (power down) this time
 *
 * \param duration sleep time in seconds
 *
 * Wake up by RTC or by buttons interrupt.
 *
 * \warning The kernel is stopped (k_cycle_get_32, k_uptime_get, ... are not
 * updated while sleeping).
 *
 */
void stm32_sleep(u32_t duration);

/**
 * \brief Disable debug pins (SWD)
 */
void stm32_swd_off(void);

/**
 * \brief Enable debug pins (SWD)
 */
void stm32_swd_on(void);

/**
 * \brief Control the 5V power supply
 *
 * \param enable zero for disable, enable otherwise
 */
void psu_5v(u32_t enable);

/**
 * \brief Control the industrial power supply
 *
 * \param enable zero for disable, enable otherwise
 */
void psu_ind(u32_t enable);

/**
 * \brief Enable the battery charger
 *
 * \param enable zero for disable, enable otherwise
 *
 * \warning Charging must not be enabled when the battery temperature is below
 * 0°C.
 */
void psu_charge(u32_t enable);

/**
 * \brief Control the CPU power supply
 *
 * \param 0 : regulated 1.8V, other values : unregulated direct battery.
 */
void psu_cpu_hp(u32_t enable);

/**
 * \brief Initialize low power
 *
 * Initialize all pins to consume less.
 */
void lp_init(void);

/**
 * \brief Prevent sleep (let the console running on the UART)
 */
void lp_sleep_prevent(void);

/**
 * \brief Allow sleep (this will prevent using the console on the UART)
 */
void lp_sleep_permit(void);

#ifdef __cplusplus
}
#endif

#endif /* STM32_LP_H */
