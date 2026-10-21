#ifndef __ADXL357_H__
#define __ADXL357_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32n6xx_hal.h"
#include <stdint.h>

/* ============================================================
 * ADXL357 Register Map
 * ============================================================ */
#define ADXL357_REG_DEVID_AD          0x00
#define ADXL357_REG_DEVID_MST         0x01
#define ADXL357_REG_PARTID            0x02
#define ADXL357_REG_REVID             0x03
#define ADXL357_REG_STATUS            0x04
#define ADXL357_REG_FIFO_ENTRIES      0x05
#define ADXL357_REG_TEMP2             0x06
#define ADXL357_REG_TEMP1             0x07

#define ADXL357_REG_XDATA3            0x08
#define ADXL357_REG_XDATA2            0x09
#define ADXL357_REG_XDATA1            0x0A
#define ADXL357_REG_YDATA3            0x0B
#define ADXL357_REG_YDATA2            0x0C
#define ADXL357_REG_YDATA1            0x0D
#define ADXL357_REG_ZDATA3            0x0E
#define ADXL357_REG_ZDATA2            0x0F
#define ADXL357_REG_ZDATA1            0x10

#define ADXL357_REG_FIFO_DATA         0x11

#define ADXL357_REG_OFFSET_X_H        0x1E
#define ADXL357_REG_OFFSET_X_L        0x1F
#define ADXL357_REG_OFFSET_Y_H        0x20
#define ADXL357_REG_OFFSET_Y_L        0x21
#define ADXL357_REG_OFFSET_Z_H        0x22
#define ADXL357_REG_OFFSET_Z_L        0x23

#define ADXL357_REG_ACT_EN            0x24
#define ADXL357_REG_ACT_THRESH_H      0x25
#define ADXL357_REG_ACT_THRESH_L      0x26
#define ADXL357_REG_ACT_COUNT         0x27

#define ADXL357_REG_FILTER            0x28
#define ADXL357_REG_FIFO_SAMPLES      0x29
#define ADXL357_REG_INT_MAP           0x2A
#define ADXL357_REG_SYNC              0x2B
#define ADXL357_REG_RANGE             0x2C
#define ADXL357_REG_POWER_CTL         0x2D
#define ADXL357_REG_SELF_TEST         0x2E
#define ADXL357_REG_RESET             0x2F


/* ============================================================
 * Output Data Rate
 * ============================================================ */
typedef enum
{
    ADXL357_ODR_4000      = 0x00,
    ADXL357_ODR_2000      = 0x01,
    ADXL357_ODR_1000      = 0x02,
    ADXL357_ODR_500       = 0x03,
    ADXL357_ODR_250       = 0x04,
    ADXL357_ODR_125       = 0x05,
    ADXL357_ODR_62_5      = 0x06,
    ADXL357_ODR_31_25     = 0x07,
    ADXL357_ODR_15_625    = 0x08,
    ADXL357_ODR_7_813     = 0x09,
    ADXL357_ODR_3_906     = 0x0A
} ADXL357_ODR_t;


/* ============================================================
 * High Pass Filter
 * ============================================================ */
typedef enum
{
    ADXL357_HPF_OFF       = 0x00,
    ADXL357_HPF_247       = 0x01,
    ADXL357_HPF_62_084    = 0x02,
    ADXL357_HPF_15_545    = 0x03,
    ADXL357_HPF_3_862     = 0x04,
    ADXL357_HPF_0_954     = 0x05,
    ADXL357_HPF_0_238     = 0x06
} ADXL357_HPF_t;


/* ============================================================
 * Range
 * ============================================================ */
typedef enum
{
    ADXL357_RANGE_10G = 0x01,
    ADXL357_RANGE_20G = 0x02,
    ADXL357_RANGE_40G = 0x03
} ADXL357_Range_t;


/* ============================================================
 * STATUS bits
 * ============================================================ */
#define ADXL357_STATUS_DATA_RDY       0x01
#define ADXL357_STATUS_FIFO_FULL      0x02
#define ADXL357_STATUS_FIFO_OVR       0x04


/* ============================================================
 * Constants
 * ============================================================ */
#define ADXL357_RESET_CODE            0x52

#define ADXL357_I2C_ADDRESS_ASEL0     0x1DU
#define ADXL357_I2C_ADDRESS_ASEL1     0x53U

#define ADXL357_MEASURE_MODE          0x00
#define ADXL357_STANDBY_MODE          0x01

#define ADXL357_XYZ_RAW_SIZE          9U

/*
 * ADXL357 FIFO_ENTRIES는 FIFO에 들어있는
 * 3-byte 데이터 word 수를 나타냅니다.
 *
 * 원본 Python:
 * read_multiple_data(REG_FIFO_DATA, fifo_entries * 3)
 */
#define ADXL357_MAX_FIFO_ENTRIES      96U
#define ADXL357_MAX_FIFO_BYTES        (ADXL357_MAX_FIFO_ENTRIES * 3U)


/* ============================================================
 * Structures
 * ============================================================ */
typedef struct
{
    int32_t x;
    int32_t y;
    int32_t z;
} ADXL357_RawAxes_t;


typedef struct
{
    float x;
    float y;
    float z;
} ADXL357_Axes_t;


typedef enum
{
    ADXL357_BUS_NONE = 0,
    ADXL357_BUS_SPI,
    ADXL357_BUS_I2C
} ADXL357_BusType_t;


typedef struct
{
    ADXL357_BusType_t bus_type;

    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;

    I2C_HandleTypeDef *hi2c;
    uint8_t i2c_address;

    float x_scale;
    float y_scale;
    float z_scale;

    uint8_t fifo_samples;

} ADXL357_Handle_t;


/* ============================================================
 * Bus-independent register access
 * ============================================================ */
HAL_StatusTypeDef ADXL357_WriteReg(
        ADXL357_Handle_t *dev,
        uint8_t reg,
        uint8_t value);

HAL_StatusTypeDef ADXL357_ReadReg(
        ADXL357_Handle_t *dev,
        uint8_t reg,
        uint8_t *value);

HAL_StatusTypeDef ADXL357_ReadMulti(
        ADXL357_Handle_t *dev,
        uint8_t reg,
        uint8_t *data,
        uint16_t length);


/* ============================================================
 * Device Control
 * ============================================================ */
HAL_StatusTypeDef ADXL357_Init(
        ADXL357_Handle_t *dev,
        SPI_HandleTypeDef *hspi,
        GPIO_TypeDef *cs_port,
        uint16_t cs_pin);

HAL_StatusTypeDef ADXL357_SPI_Init(
        ADXL357_Handle_t *dev,
        SPI_HandleTypeDef *hspi,
        GPIO_TypeDef *cs_port,
        uint16_t cs_pin);

HAL_StatusTypeDef ADXL357_I2C_Init(
        ADXL357_Handle_t *dev,
        I2C_HandleTypeDef *hi2c,
        uint8_t i2c_address);

HAL_StatusTypeDef ADXL357_Reset(ADXL357_Handle_t *dev);

HAL_StatusTypeDef ADXL357_Start(ADXL357_Handle_t *dev);

HAL_StatusTypeDef ADXL357_Stop(ADXL357_Handle_t *dev);


/* ============================================================
 * Configuration
 * ============================================================ */
HAL_StatusTypeDef ADXL357_SetRange(
        ADXL357_Handle_t *dev,
        ADXL357_Range_t range);

void ADXL357_SetSensitivity(
        ADXL357_Handle_t *dev,
        float x_sens,
        float y_sens,
        float z_sens);

HAL_StatusTypeDef ADXL357_SetFilter(
        ADXL357_Handle_t *dev,
        ADXL357_HPF_t hpf,
        ADXL357_ODR_t odr);

HAL_StatusTypeDef ADXL357_SetFifoSamples(
        ADXL357_Handle_t *dev,
        uint8_t samples);

HAL_StatusTypeDef ADXL357_EnableInterrupt(ADXL357_Handle_t *dev);

HAL_StatusTypeDef ADXL357_DisableInterrupt(ADXL357_Handle_t *dev);


/* ============================================================
 * Status
 * ============================================================ */
HAL_StatusTypeDef ADXL357_GetStatus(
        ADXL357_Handle_t *dev,
        uint8_t *status);

uint8_t ADXL357_CheckReady(ADXL357_Handle_t *dev);

uint8_t ADXL357_CheckFifoFull(ADXL357_Handle_t *dev);


/* ============================================================
 * Sensor Read
 * ============================================================ */
HAL_StatusTypeDef ADXL357_GetAxesRaw(
        ADXL357_Handle_t *dev,
        ADXL357_RawAxes_t *axes);

HAL_StatusTypeDef ADXL357_GetAxes(
        ADXL357_Handle_t *dev,
        ADXL357_Axes_t *axes);

HAL_StatusTypeDef ADXL357_GetTemperature(
        ADXL357_Handle_t *dev,
        float *temperature);


/* ============================================================
 * FIFO
 * ============================================================ */
HAL_StatusTypeDef ADXL357_GetFifoEntries(
        ADXL357_Handle_t *dev,
        uint8_t *entries);

HAL_StatusTypeDef ADXL357_ReadFifo(
        ADXL357_Handle_t *dev,
        uint8_t *buffer,
        uint16_t buffer_size,
        uint16_t *read_bytes);

uint16_t ADXL357_ConvertFifo(
        ADXL357_Handle_t *dev,
        const uint8_t *fifo_data,
        uint16_t fifo_length,
        ADXL357_Axes_t *axes,
        uint16_t max_axes);


/* ============================================================
 * Information
 * ============================================================ */
HAL_StatusTypeDef ADXL357_ReadDeviceID(
        ADXL357_Handle_t *dev,
        uint8_t *devid_ad,
        uint8_t *devid_mst,
        uint8_t *part_id);

#ifdef __cplusplus
}
#endif

#endif
