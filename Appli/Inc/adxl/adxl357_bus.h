#ifndef ADXL357_BUS_H
#define ADXL357_BUS_H

#include "adxl/adxl357.h"

HAL_StatusTypeDef ADXL357_SPI_WriteReg(
        ADXL357_Handle_t *dev, uint8_t reg, uint8_t value);
HAL_StatusTypeDef ADXL357_SPI_ReadReg(
        ADXL357_Handle_t *dev, uint8_t reg, uint8_t *value);
HAL_StatusTypeDef ADXL357_SPI_ReadMulti(
        ADXL357_Handle_t *dev, uint8_t reg, uint8_t *data, uint16_t length);

HAL_StatusTypeDef ADXL357_I2C_WriteReg(
        ADXL357_Handle_t *dev, uint8_t reg, uint8_t value);
HAL_StatusTypeDef ADXL357_I2C_ReadReg(
        ADXL357_Handle_t *dev, uint8_t reg, uint8_t *value);
HAL_StatusTypeDef ADXL357_I2C_ReadMulti(
        ADXL357_Handle_t *dev, uint8_t reg, uint8_t *data, uint16_t length);

#endif /* ADXL357_BUS_H */
