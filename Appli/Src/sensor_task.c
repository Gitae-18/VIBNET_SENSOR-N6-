#include "sensor_task.h"

#include <string.h>

#define SENSOR_TASK_DEVID_AD_EXPECTED   0xADU
#define SENSOR_TASK_DEVID_MST_EXPECTED  0x1DU
#define SENSOR_TASK_PARTID_EXPECTED     0xEDU
#define SENSOR_TASK_RESET_DELAY_MS      10U
#define SENSOR_TASK_FIFO_WORDS          72U

static void SensorTask_GetRangeSensitivity(
        ADXL357_Range_t range,
        float *sensitivity)
{
    switch (range)
    {
        case ADXL357_RANGE_10G:
            *sensitivity = 51200.0f;
            break;

        case ADXL357_RANGE_20G:
            *sensitivity = 25600.0f;
            break;

        case ADXL357_RANGE_40G:
            *sensitivity = 12800.0f;
            break;

        default:
            *sensitivity = 0.0f;
            break;
    }
}

static uint8_t SensorTask_ConfigIsValid(
        const SensorTask_Config_t *config)
{
    if (config == NULL)
    {
        return 0U;
    }

    if ((config->range < ADXL357_RANGE_10G) ||
        (config->range > ADXL357_RANGE_40G) ||
        (config->odr > ADXL357_ODR_3_906) ||
        (config->hpf > ADXL357_HPF_0_238) ||
        (config->fifo_samples == 0U) ||
        (config->fifo_samples > ADXL357_MAX_FIFO_ENTRIES) ||
        ((config->fifo_samples % 3U) != 0U) ||
        !(config->x_sensitivity > 0.0f) ||
        !(config->y_sensitivity > 0.0f) ||
        !(config->z_sensitivity > 0.0f))
    {
        return 0U;
    }

    return 1U;
}

static HAL_StatusTypeDef SensorTask_Fail(
        SensorTask_Context_t *context,
        HAL_StatusTypeDef status)
{
    if (context != NULL)
    {
        context->last_error = status;
        context->error_count++;
        context->state = SENSOR_TASK_STATE_ERROR;
    }

    SensorTask_OnError(status);
    return status;
}

static HAL_StatusTypeDef SensorTask_Configure(
        SensorTask_Context_t *context)
{
    HAL_StatusTypeDef status;

    status = ADXL357_Reset(&context->adxl357);
    if (status != HAL_OK)
    {
        return SensorTask_Fail(context, status);
    }

    HAL_Delay(SENSOR_TASK_RESET_DELAY_MS);

    status = ADXL357_ReadMulti(
            &context->adxl357,
            ADXL357_REG_DEVID_AD,
            context->device_id,
            sizeof(context->device_id));
    if (status != HAL_OK)
    {
        return SensorTask_Fail(context, status);
    }

    if ((context->device_id[0] != SENSOR_TASK_DEVID_AD_EXPECTED) ||
        (context->device_id[1] != SENSOR_TASK_DEVID_MST_EXPECTED) ||
        (context->device_id[2] != SENSOR_TASK_PARTID_EXPECTED))
    {
        return SensorTask_Fail(context, HAL_ERROR);
    }

    context->state = SENSOR_TASK_STATE_STOPPED;
    status = SensorTask_ApplyConfig(context, &context->config);
    if (status != HAL_OK)
    {
        return status;
    }

    return SensorTask_Start(context);
}

void SensorTask_GetDefaultConfig(SensorTask_Config_t *config)
{
    if (config == NULL)
    {
        return;
    }

    config->range = ADXL357_RANGE_10G;
    config->odr = ADXL357_ODR_1000;
    config->hpf = ADXL357_HPF_OFF;
    config->fifo_samples = SENSOR_TASK_FIFO_WORDS;
    config->x_sensitivity = 51200.0f;
    config->y_sensitivity = 51200.0f;
    config->z_sensitivity = 51200.0f;
}

HAL_StatusTypeDef SensorTask_GetConfig(
        const SensorTask_Context_t *context,
        SensorTask_Config_t *config)
{
    if ((context == NULL) || (config == NULL) ||
        (context->state == SENSOR_TASK_STATE_UNINITIALIZED))
    {
        return HAL_ERROR;
    }

    *config = context->config;
    return HAL_OK;
}

HAL_StatusTypeDef SensorTask_ApplyConfig(
        SensorTask_Context_t *context,
        const SensorTask_Config_t *config)
{
    HAL_StatusTypeDef status;
    uint8_t was_running;

    if ((context == NULL) ||
        (context->state == SENSOR_TASK_STATE_UNINITIALIZED) ||
        (context->state == SENSOR_TASK_STATE_ERROR) ||
        (SensorTask_ConfigIsValid(config) == 0U))
    {
        return HAL_ERROR;
    }

    was_running = (context->state == SENSOR_TASK_STATE_RUNNING) ? 1U : 0U;
    if (was_running != 0U)
    {
        status = ADXL357_DisableInterrupt(&context->adxl357);
        if (status != HAL_OK)
        {
            return SensorTask_Fail(context, status);
        }

        status = ADXL357_Stop(&context->adxl357);
        if (status != HAL_OK)
        {
            return SensorTask_Fail(context, status);
        }

        context->state = SENSOR_TASK_STATE_STOPPED;
    }

    status = ADXL357_SetRange(&context->adxl357, config->range);
    if (status != HAL_OK)
    {
        return SensorTask_Fail(context, status);
    }

    status = ADXL357_SetFilter(
            &context->adxl357,
            config->hpf,
            config->odr);
    if (status != HAL_OK)
    {
        return SensorTask_Fail(context, status);
    }

    status = ADXL357_SetFifoSamples(
            &context->adxl357,
            config->fifo_samples);
    if (status != HAL_OK)
    {
        return SensorTask_Fail(context, status);
    }

    ADXL357_SetSensitivity(
            &context->adxl357,
            config->x_sensitivity,
            config->y_sensitivity,
            config->z_sensitivity);

    context->config = *config;
    context->sample_valid = 0U;
    context->last_error = HAL_OK;

    if (was_running != 0U)
    {
        return SensorTask_Start(context);
    }

    return HAL_OK;
}

HAL_StatusTypeDef SensorTask_SetRange(
        SensorTask_Context_t *context,
        ADXL357_Range_t range)
{
    SensorTask_Config_t config;
    float sensitivity;

    if (SensorTask_GetConfig(context, &config) != HAL_OK)
    {
        return HAL_ERROR;
    }

    SensorTask_GetRangeSensitivity(range, &sensitivity);
    if (!(sensitivity > 0.0f))
    {
        return HAL_ERROR;
    }

    config.range = range;
    config.x_sensitivity = sensitivity;
    config.y_sensitivity = sensitivity;
    config.z_sensitivity = sensitivity;
    return SensorTask_ApplyConfig(context, &config);
}

HAL_StatusTypeDef SensorTask_SetFilter(
        SensorTask_Context_t *context,
        ADXL357_HPF_t hpf,
        ADXL357_ODR_t odr)
{
    SensorTask_Config_t config;

    if (SensorTask_GetConfig(context, &config) != HAL_OK)
    {
        return HAL_ERROR;
    }

    config.hpf = hpf;
    config.odr = odr;
    return SensorTask_ApplyConfig(context, &config);
}

HAL_StatusTypeDef SensorTask_SetSensitivity(
        SensorTask_Context_t *context,
        float x_sensitivity,
        float y_sensitivity,
        float z_sensitivity)
{
    SensorTask_Config_t config;

    if (SensorTask_GetConfig(context, &config) != HAL_OK)
    {
        return HAL_ERROR;
    }

    config.x_sensitivity = x_sensitivity;
    config.y_sensitivity = y_sensitivity;
    config.z_sensitivity = z_sensitivity;
    return SensorTask_ApplyConfig(context, &config);
}

HAL_StatusTypeDef SensorTask_Start(SensorTask_Context_t *context)
{
    HAL_StatusTypeDef status;

    if ((context == NULL) ||
        (context->state == SENSOR_TASK_STATE_UNINITIALIZED) ||
        (context->state == SENSOR_TASK_STATE_ERROR))
    {
        return HAL_ERROR;
    }

    if (context->state == SENSOR_TASK_STATE_RUNNING)
    {
        return HAL_OK;
    }

    status = ADXL357_EnableInterrupt(&context->adxl357);
    if (status != HAL_OK)
    {
        return SensorTask_Fail(context, status);
    }

    status = ADXL357_Start(&context->adxl357);
    if (status != HAL_OK)
    {
        return SensorTask_Fail(context, status);
    }

    context->sample_valid = 0U;
    context->last_error = HAL_OK;
    context->state = SENSOR_TASK_STATE_RUNNING;
    return HAL_OK;
}

HAL_StatusTypeDef SensorTask_ProcessFifo(
        SensorTask_Context_t *context,
        SensorTask_FifoBlock_t *block)
{
    HAL_StatusTypeDef status;
    uint8_t sensor_status;
    uint16_t converted;

    if ((context == NULL) || (block == NULL) ||
        (context->state != SENSOR_TASK_STATE_RUNNING))
    {
        return HAL_ERROR;
    }

    block->data_length = 0U;
    block->sample_count = 0U;

    status = ADXL357_GetStatus(&context->adxl357, &sensor_status);
    if (status != HAL_OK)
    {
        return SensorTask_Fail(context, status);
    }

    if ((sensor_status & ADXL357_STATUS_FIFO_FULL) == 0U)
    {
        return HAL_OK;
    }

    block->timestamp_ms = HAL_GetTick();
    status = ADXL357_ReadFifo(
            &context->adxl357,
            block->fifo_data,
            sizeof(block->fifo_data),
            &block->data_length);
    if (status != HAL_OK)
    {
        return SensorTask_Fail(context, status);
    }

    block->sample_count =
            block->data_length / ADXL357_XYZ_RAW_SIZE;
    if (block->sample_count == 0U)
    {
        return HAL_OK;
    }

    /* Keep the newest FIFO sample available through SensorTask_GetLatest(). */
    converted = ADXL357_ConvertFifo(
            &context->adxl357,
            &block->fifo_data[block->data_length - ADXL357_XYZ_RAW_SIZE],
            ADXL357_XYZ_RAW_SIZE,
            &context->latest_axes,
            1U);
    if (converted == 1U)
    {
        context->sample_valid = 1U;
        SensorTask_OnSample(&context->latest_axes);
    }

    context->sample_count += block->sample_count;
    context->last_error = HAL_OK;
    SensorTask_OnFifo(block);

    return HAL_OK;
}

HAL_StatusTypeDef SensorTask_InitI2C(
        SensorTask_Context_t *context,
        I2C_HandleTypeDef *hi2c,
        uint8_t address)
{
    HAL_StatusTypeDef status;

    if ((context == NULL) || (hi2c == NULL))
    {
        return HAL_ERROR;
    }

    memset(context, 0, sizeof(*context));
    SensorTask_GetDefaultConfig(&context->config);
    status = ADXL357_I2C_Init(&context->adxl357, hi2c, address);
    if (status != HAL_OK)
    {
        return SensorTask_Fail(context, status);
    }

    return SensorTask_Configure(context);
}

HAL_StatusTypeDef SensorTask_InitSPI(
        SensorTask_Context_t *context,
        SPI_HandleTypeDef *hspi,
        GPIO_TypeDef *cs_port,
        uint16_t cs_pin)
{
    HAL_StatusTypeDef status;

    if ((context == NULL) || (hspi == NULL) || (cs_port == NULL))
    {
        return HAL_ERROR;
    }

    memset(context, 0, sizeof(*context));
    SensorTask_GetDefaultConfig(&context->config);
    status = ADXL357_SPI_Init(
            &context->adxl357,
            hspi,
            cs_port,
            cs_pin);
    if (status != HAL_OK)
    {
        return SensorTask_Fail(context, status);
    }

    return SensorTask_Configure(context);
}

HAL_StatusTypeDef SensorTask_Process(SensorTask_Context_t *context)
{
    HAL_StatusTypeDef status;
    uint8_t sensor_status;

    if ((context == NULL) ||
        (context->state != SENSOR_TASK_STATE_RUNNING))
    {
        return HAL_ERROR;
    }

    status = ADXL357_GetStatus(&context->adxl357, &sensor_status);
    if (status != HAL_OK)
    {
        return SensorTask_Fail(context, status);
    }

    if ((sensor_status & ADXL357_STATUS_DATA_RDY) == 0U)
    {
        return HAL_OK;
    }

    status = ADXL357_GetAxes(&context->adxl357, &context->latest_axes);
    if (status != HAL_OK)
    {
        return SensorTask_Fail(context, status);
    }

    context->sample_valid = 1U;
    context->sample_count++;
    context->last_error = HAL_OK;
    SensorTask_OnSample(&context->latest_axes);

    return HAL_OK;
}

HAL_StatusTypeDef SensorTask_GetLatest(
        const SensorTask_Context_t *context,
        ADXL357_Axes_t *axes)
{
    if ((context == NULL) || (axes == NULL) ||
        (context->sample_valid == 0U))
    {
        return HAL_ERROR;
    }

    *axes = context->latest_axes;
    return HAL_OK;
}

void SensorTask_Stop(SensorTask_Context_t *context)
{
    HAL_StatusTypeDef status;

    if (context == NULL)
    {
        return;
    }

    if (context->state == SENSOR_TASK_STATE_RUNNING)
    {
        status = ADXL357_DisableInterrupt(&context->adxl357);
        if (status == HAL_OK)
        {
            status = ADXL357_Stop(&context->adxl357);
        }

        context->last_error = status;
    }

    context->sample_valid = 0U;
    context->state = SENSOR_TASK_STATE_STOPPED;
}

__weak void SensorTask_OnSample(const ADXL357_Axes_t *axes)
{
    (void)axes;
}

__weak void SensorTask_OnFifo(const SensorTask_FifoBlock_t *block)
{
    (void)block;
}

__weak void SensorTask_OnError(HAL_StatusTypeDef status)
{
    (void)status;
}
