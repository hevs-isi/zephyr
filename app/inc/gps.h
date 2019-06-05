#ifndef GPS_H
#define GPS_H

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * \brief Initialize GPS
 */
void gps_init(void);

/**
 * \brief Turn gps off (including RX/TX/PPS pins)
 */
void gps_off(void);

#ifdef __cplusplus
}
#endif

#endif /* GPS_H */
