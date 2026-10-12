#include "ina219.h"

#define INA219_TIMEOUT_MS      100U

#define INA219_CONFIG_RESET    0x8000U
#define INA219_CONFIG_DEFAULT  0x399FU

#define INA219_BUS_CNVR        0x0002U
#define INA219_BUS_OVF         0x0001U

uint16_t ina219_calib_value(const ina219_t *dev)
{
    uint64_t cal = 40960000000ULL / ((uint64_t)dev->current_lsb_ua * dev->shunt_uohm);
    if (cal > 0xFFFEU) {
        cal = 0xFFFEU;
    }
    return (uint16_t)(cal & 0xFFFEU);
}

HAL_StatusTypeDef ina219_read_reg(const ina219_t *dev, uint8_t reg, uint16_t *value)
{
    uint8_t buf[2];
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(dev->hi2c, dev->addr, reg, I2C_MEMADD_SIZE_8BIT, buf, sizeof(buf),
                                            INA219_TIMEOUT_MS);
    if (st == HAL_OK) {
        *value = (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
    }
    return st;
}

HAL_StatusTypeDef ina219_write_reg(const ina219_t *dev, uint8_t reg, uint16_t value)
{
    uint8_t buf[2] = { (uint8_t)(value >> 8), (uint8_t)value };
    return HAL_I2C_Mem_Write(dev->hi2c, dev->addr, reg, I2C_MEMADD_SIZE_8BIT, buf, sizeof(buf), INA219_TIMEOUT_MS);
}

static HAL_StatusTypeDef ina219_write_verify(const ina219_t *dev, uint8_t reg, uint16_t value)
{
    HAL_StatusTypeDef st = ina219_write_reg(dev, reg, value);
    if (st != HAL_OK) {
        return st;
    }
    uint16_t readback = 0U;
    st = ina219_read_reg(dev, reg, &readback);
    if (st != HAL_OK) {
        return st;
    }
    return (readback == value) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef ina219_init(const ina219_t *dev)
{
    if (HAL_I2C_IsDeviceReady(dev->hi2c, dev->addr, 3U, INA219_TIMEOUT_MS) != HAL_OK) {
        return HAL_ERROR;
    }
    HAL_StatusTypeDef st = ina219_write_reg(dev, INA219_REG_CONFIG, INA219_CONFIG_RESET);
    if (st != HAL_OK) {
        return st;
    }
    HAL_Delay(1U);
    uint16_t config = 0U;
    st = ina219_read_reg(dev, INA219_REG_CONFIG, &config);
    if (st != HAL_OK) {
        return st;
    }
    if (config != INA219_CONFIG_DEFAULT) {
        return HAL_ERROR;
    }
    st = ina219_write_verify(dev, INA219_REG_CALIB, ina219_calib_value(dev));
    if (st != HAL_OK) {
        return st;
    }
    return ina219_write_verify(dev, INA219_REG_CONFIG, dev->config);
}

HAL_StatusTypeDef ina219_poll(const ina219_t *dev, ina219_reading_t *out)
{
    uint16_t bus = 0U, shunt = 0U, current = 0U, power = 0U;
    out->ready = false;
    HAL_StatusTypeDef st = ina219_read_reg(dev, INA219_REG_BUS, &bus);
    if (st != HAL_OK || (bus & INA219_BUS_CNVR) == 0U) {
        return st;
    }
    st = ina219_read_reg(dev, INA219_REG_SHUNT, &shunt);
    if (st == HAL_OK) {
        st = ina219_read_reg(dev, INA219_REG_CURRENT, &current);
    }
    if (st == HAL_OK) {
        st = ina219_read_reg(dev, INA219_REG_POWER, &power);
    }
    if (st != HAL_OK) {
        return st;
    }
    out->ready = true;
    out->shunt_uv = (int32_t)(int16_t)shunt * 10;
    out->bus_mv = (uint32_t)(bus >> 3) * 4U;
    out->current_ua = (int32_t)(int16_t)current * (int32_t)dev->current_lsb_ua;
    out->power_uw = (uint32_t)power * 20U * dev->current_lsb_ua;
    out->overflow = (bus & INA219_BUS_OVF) != 0U;
    return HAL_OK;
}

bool ina219_calibration_ok(const ina219_t *dev)
{
    uint16_t calib = 0U;
    return ina219_read_reg(dev, INA219_REG_CALIB, &calib) == HAL_OK && calib == ina219_calib_value(dev);
}
