#include "adxl/adxl357_bus.h"

#define ADXL357_I2C_TIMEOUT_MS 100U

static uint16_t ADXL357_I2C_DeviceAddress(const ADXL357_Handle_t *dev)
{
    return (uint16_t)((uint16_t)dev->i2c_address << 1);
}

HAL_StatusTypeDef ADXL357_I2C_WriteReg(
        ADXL357_Handle_t *dev, uint8_t reg, uint8_t value)
{
    if ((dev == NULL) || (dev->hi2c == NULL) ||
        (dev->bus_type != ADXL357_BUS_I2C))
    {
        return HAL_ERROR;
    }

    return HAL_I2C_Mem_Write(dev->hi2c,
                             ADXL357_I2C_DeviceAddress(dev),
                             reg,
                             I2C_MEMADD_SIZE_8BIT,
                             &value,
                             1U,
                             ADXL357_I2C_TIMEOUT_MS);
}

HAL_StatusTypeDef ADXL357_I2C_ReadReg(
        ADXL357_Handle_t *dev, uint8_t reg, uint8_t *value)
{
    if ((dev == NULL) || (value == NULL) || (dev->hi2c == NULL) ||
        (dev->bus_type != ADXL357_BUS_I2C))
    {
        return HAL_ERROR;
    }

    return HAL_I2C_Mem_Read(dev->hi2c,
                            ADXL357_I2C_DeviceAddress(dev),
                            reg,
                            I2C_MEMADD_SIZE_8BIT,
                            value,
                            1U,
                            ADXL357_I2C_TIMEOUT_MS);
}

HAL_StatusTypeDef ADXL357_I2C_ReadMulti(
        ADXL357_Handle_t *dev, uint8_t reg, uint8_t *data, uint16_t length)
{
    if ((dev == NULL) || (data == NULL) || (length == 0U) ||
        (dev->hi2c == NULL) || (dev->bus_type != ADXL357_BUS_I2C))
    {
        return HAL_ERROR;
    }

    return HAL_I2C_Mem_Read(dev->hi2c,
                            ADXL357_I2C_DeviceAddress(dev),
                            reg,
                            I2C_MEMADD_SIZE_8BIT,
                            data,
                            length,
                            ADXL357_I2C_TIMEOUT_MS);
}
