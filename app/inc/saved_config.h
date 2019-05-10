#ifndef SAVED_CONFIG_H
#define SAVED_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

enum psu_e
{
	PSU_NONE	= 0x00,
	PSU_5V		= 0x01,
	PSU_INDUS	= 0x02,
};

enum mode_e
{
	MODE_0_10V		= 0x00,
	MODE_4_20MA		= 0x01,
	MODE_DIGITAL	= 0x02,
	MODE_I2C		= 0x03,
};

enum tx_mode_e
{
	TX_MODE_LAST	= 0x00,
	TX_MODE_MEAN	= 0x01,
	TX_MODE_MAX		= 0x02,
	TX_MODE_MIN		= 0x03,
};

struct sensor_config_t
{
	uint8_t enable;
	uint8_t psu;
	uint8_t mode;
	uint8_t tx_mode;
	uint32_t period;
	uint32_t wakeup_ms;
	uint8_t tx_period;
};

struct saved_config_t
{
	uint32_t version;
	uint32_t boot_count;
	uint32_t changes;
	struct sensor_config_t a;
	struct sensor_config_t b;
	uint8_t _reserved[2*64-(16+2*sizeof(struct sensor_config_t))];
	uint32_t crc;
};

void saved_config_init(void);

int saved_config_read(struct saved_config_t *config);
int saved_config_save(struct saved_config_t *config);

enum saved_config_predef_e
{
	SC_DEFAULT_INDUS_1	= 0x00,
	SC_DEFAULT_INDUS_2	= 0x01,
	SC_DEFAULT_INDUS_1_FAST	= 0x02,
	SC_DEFAULT_INDUS_2_FAST	= 0x03,
};

void saved_config_predef(struct saved_config_t *config, enum saved_config_predef_e pre);

#ifdef __cplusplus
}
#endif

#endif /* SAVED_CONFIG_H */
