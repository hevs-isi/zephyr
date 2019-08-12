#ifndef DATALOGGER_H
#define DATALOGGER_H

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * \brief Initialize the datalogger
 */
int datalogger_init(void);

/**
 * \brief nprintf like datalogger
 */
int datalogger_nprintf(size_t n, const char *fmt, ...);

/**
 * \brief Force sync
 *
 * Maybe a good idea before sleeping.
 */
int datalogger_sync(void);

/**
 * \brief Close the current file
 */
int datalogger_close(void);

#ifdef __cplusplus
}
#endif

#endif /* DATALOGGER_H */
