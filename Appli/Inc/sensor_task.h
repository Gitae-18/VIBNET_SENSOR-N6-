#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "adxl/adxl357.h"

typedef enum
{
    SENSOR_TASK_STATE_UNINITIALIZED = 0,
    SENSOR_TASK_STATE_STOPPED,
    SENSOR_TASK_STATE_RUNNING,
    SENSOR_TASK_STATE_ERROR
} SensorTask_State_t;

typedef struct
{
    ADXL357_Range_t range;
    ADXL357_ODR_t odr;
    ADXL357_HPF_t hpf;
    uint8_t fifo_samples;
    float x_sensitivity;
    float y_sensitivity;
    float z_sensitivity;
} SensorTask_Config_t;

typedef struct
{
    ADXL357_Handle_t adxl357;
    SensorTask_Config_t config;
    ADXL357_Axes_t latest_axes;
    uint8_t device_id[4];
    uint32_t sample_count;
    uint32_t error_count;
    HAL_StatusTypeDef last_error;
    SensorTask_State_t state;
    uint8_t sample_valid;
} SensorTask_Context_t;

typedef struct
{
    uint32_t timestamp_ms;
    uint16_t data_length;
    uint16_t sample_count;
    uint8_t fifo_data[ADXL357_MAX_FIFO_BYTES];
} SensorTask_FifoBlock_t;

HAL_StatusTypeDef SensorTask_InitI2C(
        SensorTask_Context_t *context,
        I2C_HandleTypeDef *hi2c,
        uint8_t address);

HAL_StatusTypeDef SensorTask_InitSPI(
        SensorTask_Context_t *context,
        SPI_HandleTypeDef *hspi,
        GPIO_TypeDef *cs_port,
        uint16_t cs_pin);

void SensorTask_GetDefaultConfig(SensorTask_Config_t *config);

HAL_StatusTypeDef SensorTask_GetConfig(
        const SensorTask_Context_t *context,
        SensorTask_Config_t *config);

HAL_StatusTypeDef SensorTask_ApplyConfig(
        SensorTask_Context_t *context,
        const SensorTask_Config_t *config);

HAL_StatusTypeDef SensorTask_SetRange(
        SensorTask_Context_t *context,
        ADXL357_Range_t range);

HAL_StatusTypeDef SensorTask_SetFilter(
        SensorTask_Context_t *context,
        ADXL357_HPF_t hpf,
        ADXL357_ODR_t odr);

HAL_StatusTypeDef SensorTask_SetSensitivity(
        SensorTask_Context_t *context,
        float x_sensitivity,
        float y_sensitivity,
        float z_sensitivity);

HAL_StatusTypeDef SensorTask_Start(SensorTask_Context_t *context);

HAL_StatusTypeDef SensorTask_Process(SensorTask_Context_t *context);

HAL_StatusTypeDef SensorTask_ProcessFifo(
        SensorTask_Context_t *context,
        SensorTask_FifoBlock_t *block);

HAL_StatusTypeDef SensorTask_GetLatest(
        const SensorTask_Context_t *context,
        ADXL357_Axes_t *axes);

void SensorTask_Stop(SensorTask_Context_t *context);

/*
 * Override these weak callbacks in another source file when samples or errors
 * must be forwarded to a queue, network task, logger, etc.
 */
void SensorTask_OnSample(const ADXL357_Axes_t *axes);
void SensorTask_OnFifo(const SensorTask_FifoBlock_t *block);
void SensorTask_OnError(HAL_StatusTypeDef status);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_TASK_H */
