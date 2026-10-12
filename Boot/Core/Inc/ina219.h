#ifndef INA219_H
#define INA219_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "main.h"

#define INA219_REG_CONFIG     0x00U
#define INA219_REG_SHUNT      0x01U
#define INA219_REG_BUS        0x02U
#define INA219_REG_POWER      0x03U
#define INA219_REG_CURRENT    0x04U
#define INA219_REG_CALIB      0x05U

#define INA219_BRNG_16V       (0U << 13)
#define INA219_BRNG_32V       (1U << 13)
#define INA219_PGA_40MV       (0U << 11)
#define INA219_PGA_80MV       (1U << 11)
#define INA219_PGA_160MV      (2U << 11)
#define INA219_PGA_320MV      (3U << 11)
#define INA219_BADC_12BIT     (0x3U << 7)
#define INA219_BADC_AVG128    (0xFU << 7)
#define INA219_SADC_12BIT     (0x3U << 3)
#define INA219_SADC_AVG128    (0xFU << 3)
#define INA219_MODE_CONT      0x7U

typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint16_t addr;
    uint32_t shunt_uohm;
    uint32_t current_lsb_ua;
    uint16_t config;
} ina219_t;

typedef struct {
    bool     ready;
    int32_t  shunt_uv;
    uint32_t bus_mv;
    int32_t  current_ua;
    uint32_t power_uw;
    bool     overflow;
} ina219_reading_t;

HAL_StatusTypeDef ina219_init(const ina219_t *dev);
HAL_StatusTypeDef ina219_poll(const ina219_t *dev, ina219_reading_t *out);
bool ina219_calibration_ok(const ina219_t *dev);
uint16_t ina219_calib_value(const ina219_t *dev);
HAL_StatusTypeDef ina219_read_reg(const ina219_t *dev, uint8_t reg, uint16_t *value);
HAL_StatusTypeDef ina219_write_reg(const ina219_t *dev, uint8_t reg, uint16_t value);

#ifdef __cplusplus
}
#endif

#endif /* INA219_H */
