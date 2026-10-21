#include "adxl/adxl357_bus.h"

#define ADXL357_SPI_TIMEOUT_MS 100U

static void ADXL357_SPI_Select(const ADXL357_Handle_t *dev)
{
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_RESET);
}

static void ADXL357_SPI_Deselect(const ADXL357_Handle_t *dev)
{
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_SET);
}

HAL_StatusTypeDef ADXL357_SPI_WriteReg(
        ADXL357_Handle_t *dev, uint8_t reg, uint8_t value)
{
    uint8_t tx[2];
    HAL_StatusTypeDef status;

    if ((dev == NULL) || (dev->hspi == NULL) ||
        (dev->cs_port == NULL) || (dev->bus_type != ADXL357_BUS_SPI))
    {
        return HAL_ERROR;
    }

    tx[0] = (uint8_t)(reg << 1);
    tx[1] = value;

    ADXL357_SPI_Select(dev);
    status = HAL_SPI_Transmit(dev->hspi, tx, sizeof(tx), ADXL357_SPI_TIMEOUT_MS);
    ADXL357_SPI_Deselect(dev);

    return status;
}

HAL_StatusTypeDef ADXL357_SPI_ReadReg(
        ADXL357_Handle_t *dev, uint8_t reg, uint8_t *value)
{
    uint8_t tx[2];
    uint8_t rx[2] = {0U, 0U};
    HAL_StatusTypeDef status;

    if ((dev == NULL) || (value == NULL) || (dev->hspi == NULL) ||
        (dev->cs_port == NULL) || (dev->bus_type != ADXL357_BUS_SPI))
    {
        return HAL_ERROR;
    }

    tx[0] = (uint8_t)((reg << 1) | 0x01U);
    tx[1] = 0U;

    ADXL357_SPI_Select(dev);
    status = HAL_SPI_TransmitReceive(dev->hspi, tx, rx, sizeof(tx),
                                     ADXL357_SPI_TIMEOUT_MS);
    ADXL357_SPI_Deselect(dev);

    if (status == HAL_OK)
    {
        *value = rx[1];
    }

    return status;
}

HAL_StatusTypeDef ADXL357_SPI_ReadMulti(
        ADXL357_Handle_t *dev, uint8_t reg, uint8_t *data, uint16_t length)
{
    uint8_t command;
    HAL_StatusTypeDef status;

    if ((dev == NULL) || (data == NULL) || (length == 0U) ||
        (dev->hspi == NULL) || (dev->cs_port == NULL) ||
        (dev->bus_type != ADXL357_BUS_SPI))
    {
        return HAL_ERROR;
    }

    command = (uint8_t)((reg << 1) | 0x01U);

    ADXL357_SPI_Select(dev);
    status = HAL_SPI_Transmit(dev->hspi, &command, 1U,
                              ADXL357_SPI_TIMEOUT_MS);
    if (status == HAL_OK)
    {
        status = HAL_SPI_Receive(dev->hspi, data, length,
                                 ADXL357_SPI_TIMEOUT_MS);
    }
    ADXL357_SPI_Deselect(dev);

    return status;
}
