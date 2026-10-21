#include "adxl357.h"


/* ============================================================
 * Internal Functions
 * ============================================================ */

static inline void ADXL357_CS_Low(ADXL357_Handle_t *dev)
{
    HAL_GPIO_WritePin(
            dev->cs_port,
            dev->cs_pin,
            GPIO_PIN_RESET);
}


static inline void ADXL357_CS_High(ADXL357_Handle_t *dev)
{
    HAL_GPIO_WritePin(
            dev->cs_port,
            dev->cs_pin,
            GPIO_PIN_SET);
}


/*
 * ADXL357 20-bit two's complement 변환
 *
 * Python:
 *
 * raw = (byte0 << 12)
 *     | (byte1 << 4)
 *     | (byte2 >> 4)
 */
static int32_t ADXL357_Convert20Bit(const uint8_t *data)
{
    int32_t raw;

    raw =
        ((int32_t)data[0] << 12) |
        ((int32_t)data[1] << 4)  |
        ((int32_t)data[2] >> 4);

    /*
     * bit19 = sign bit
     */
    if (raw & 0x80000)
    {
        raw |= (int32_t)0xFFF00000;
    }

    return raw;
}


/* ============================================================
 * SPI Access
 * ============================================================ */

HAL_StatusTypeDef ADXL357_WriteReg(
        ADXL357_Handle_t *dev,
        uint8_t reg,
        uint8_t value)
{
    uint8_t tx[2];

    /*
     * Python:
     * w_address = address << 1
     */
    tx[0] = (uint8_t)(reg << 1);
    tx[1] = value;

    ADXL357_CS_Low(dev);

    HAL_StatusTypeDef ret =
        HAL_SPI_Transmit(
                dev->hspi,
                tx,
                sizeof(tx),
                HAL_MAX_DELAY);

    ADXL357_CS_High(dev);

    return ret;
}


HAL_StatusTypeDef ADXL357_ReadReg(
        ADXL357_Handle_t *dev,
        uint8_t reg,
        uint8_t *value)
{
    HAL_StatusTypeDef ret;

    uint8_t tx[2];
    uint8_t rx[2];

    /*
     * Python:
     *
     * r_address = address << 1 | 0x01
     */
    tx[0] = (uint8_t)((reg << 1) | 0x01);
    tx[1] = 0x00;

    rx[0] = 0;
    rx[1] = 0;

    ADXL357_CS_Low(dev);

    ret = HAL_SPI_TransmitReceive(
            dev->hspi,
            tx,
            rx,
            sizeof(tx),
            HAL_MAX_DELAY);

    ADXL357_CS_High(dev);

    if (ret == HAL_OK)
    {
        *value = rx[1];
    }

    return ret;
}


HAL_StatusTypeDef ADXL357_ReadMulti(
        ADXL357_Handle_t *dev,
        uint8_t reg,
        uint8_t *data,
        uint16_t length)
{
    HAL_StatusTypeDef ret;

    uint8_t command =
        (uint8_t)((reg << 1) | 0x01);

    ADXL357_CS_Low(dev);

    /*
     * Register address 전송
     */
    ret = HAL_SPI_Transmit(
            dev->hspi,
            &command,
            1,
            HAL_MAX_DELAY);

    /*
     * 연속 데이터 수신
     */
    if (ret == HAL_OK)
    {
        ret = HAL_SPI_Receive(
                dev->hspi,
                data,
                length,
                HAL_MAX_DELAY);
    }

    ADXL357_CS_High(dev);

    return ret;
}


/* ============================================================
 * Initialization
 * ============================================================ */

HAL_StatusTypeDef ADXL357_Init(
        ADXL357_Handle_t *dev,
        SPI_HandleTypeDef *hspi,
        GPIO_TypeDef *cs_port,
        uint16_t cs_pin)
{
    if ((dev == NULL) ||
        (hspi == NULL) ||
        (cs_port == NULL))
    {
        return HAL_ERROR;
    }

    dev->hspi = hspi;
    dev->cs_port = cs_port;
    dev->cs_pin = cs_pin;

    /*
     * Python 기본값:
     *
     * self._x_scale = 1 / 12800
     * self._y_scale = 1 / 12800
     * self._z_scale = 1 / 12800
     *
     * ±40g 기준
     */
    dev->x_scale = 1.0f / 12800.0f;
    dev->y_scale = 1.0f / 12800.0f;
    dev->z_scale = 1.0f / 12800.0f;

    dev->fifo_samples = 0;

    ADXL357_CS_High(dev);

    return HAL_OK;
}


/* ============================================================
 * Reset / Start / Stop
 * ============================================================ */

HAL_StatusTypeDef ADXL357_Reset(
        ADXL357_Handle_t *dev)
{
    return ADXL357_WriteReg(
            dev,
            ADXL357_REG_RESET,
            ADXL357_RESET_CODE);
}


HAL_StatusTypeDef ADXL357_Start(
        ADXL357_Handle_t *dev)
{
    uint8_t data;

    HAL_StatusTypeDef ret =
        ADXL357_ReadReg(
                dev,
                ADXL357_REG_POWER_CTL,
                &data);

    if (ret != HAL_OK)
    {
        return ret;
    }

    /*
     * Python 원본의 목적은
     * STANDBY bit를 0으로 만들어 Measurement Mode로 진입하는 것.
     */
    data &= (uint8_t)(~ADXL357_STANDBY_MODE);

    return ADXL357_WriteReg(
            dev,
            ADXL357_REG_POWER_CTL,
            data);
}


HAL_StatusTypeDef ADXL357_Stop(
        ADXL357_Handle_t *dev)
{
    uint8_t data;

    HAL_StatusTypeDef ret =
        ADXL357_ReadReg(
                dev,
                ADXL357_REG_POWER_CTL,
                &data);

    if (ret != HAL_OK)
    {
        return ret;
    }

    data |= ADXL357_STANDBY_MODE;

    return ADXL357_WriteReg(
            dev,
            ADXL357_REG_POWER_CTL,
            data);
}


/* ============================================================
 * Sensitivity / Range
 * ============================================================ */

void ADXL357_SetSensitivity(
        ADXL357_Handle_t *dev,
        float x_sens,
        float y_sens,
        float z_sens)
{
    if (x_sens != 0.0f)
    {
        dev->x_scale = 1.0f / x_sens;
    }

    if (y_sens != 0.0f)
    {
        dev->y_scale = 1.0f / y_sens;
    }

    if (z_sens != 0.0f)
    {
        dev->z_scale = 1.0f / z_sens;
    }
}


HAL_StatusTypeDef ADXL357_SetRange(
        ADXL357_Handle_t *dev,
        ADXL357_Range_t range)
{
    uint8_t range_data;

    HAL_StatusTypeDef ret =
        ADXL357_ReadReg(
                dev,
                ADXL357_REG_RANGE,
                &range_data);

    if (ret != HAL_OK)
    {
        return ret;
    }

    /*
     * Python:
     *
     * reg_range | (range_data & 0xC0)
     */
    range_data =
        (uint8_t)range |
        (range_data & 0xC0);

    ret = ADXL357_WriteReg(
            dev,
            ADXL357_REG_RANGE,
            range_data);

    if (ret != HAL_OK)
    {
        return ret;
    }

    /*
     * Python 코드와 대응되는 기본 sensitivity
     */
    switch (range)
    {
        case ADXL357_RANGE_10G:

            ADXL357_SetSensitivity(
                    dev,
                    51200.0f,
                    51200.0f,
                    51200.0f);

            break;

        case ADXL357_RANGE_20G:

            ADXL357_SetSensitivity(
                    dev,
                    25600.0f,
                    25600.0f,
                    25600.0f);

            break;

        case ADXL357_RANGE_40G:

            ADXL357_SetSensitivity(
                    dev,
                    12800.0f,
                    12800.0f,
                    12800.0f);

            break;

        default:
            return HAL_ERROR;
    }

    return HAL_OK;
}


/* ============================================================
 * Filter / ODR
 * ============================================================ */

HAL_StatusTypeDef ADXL357_SetFilter(
        ADXL357_Handle_t *dev,
        ADXL357_HPF_t hpf,
        ADXL357_ODR_t odr)
{
    uint8_t value;

    /*
     * Python:
     *
     * (hpf << 4) | lpf
     */
    value =
        ((uint8_t)hpf << 4) |
        ((uint8_t)odr & 0x0F);

    return ADXL357_WriteReg(
            dev,
            ADXL357_REG_FILTER,
            value);
}


/* ============================================================
 * FIFO
 * ============================================================ */

HAL_StatusTypeDef ADXL357_SetFifoSamples(
        ADXL357_Handle_t *dev,
        uint8_t samples)
{
    dev->fifo_samples = samples;

    return ADXL357_WriteReg(
            dev,
            ADXL357_REG_FIFO_SAMPLES,
            samples);
}


HAL_StatusTypeDef ADXL357_GetFifoEntries(
        ADXL357_Handle_t *dev,
        uint8_t *entries)
{
    return ADXL357_ReadReg(
            dev,
            ADXL357_REG_FIFO_ENTRIES,
            entries);
}


HAL_StatusTypeDef ADXL357_ReadFifo(
        ADXL357_Handle_t *dev,
        uint8_t *buffer,
        uint16_t buffer_size,
        uint16_t *read_bytes)
{
    uint8_t entries;

    HAL_StatusTypeDef ret =
        ADXL357_GetFifoEntries(
                dev,
                &entries);

    if (ret != HAL_OK)
    {
        return ret;
    }

    if (entries == 0)
    {
        *read_bytes = 0;
        return HAL_OK;
    }

    /*
     * Python 원본:
     *
     * read_multiple_data(
     *      REG_FIFO_DATA,
     *      fifo_entries * 3)
     */
    uint16_t bytes =
        (uint16_t)entries * 3U;

    if (bytes > buffer_size)
    {
        bytes = buffer_size;
    }

    ret = ADXL357_ReadMulti(
            dev,
            ADXL357_REG_FIFO_DATA,
            buffer,
            bytes);

    if (ret == HAL_OK)
    {
        *read_bytes = bytes;
    }
    else
    {
        *read_bytes = 0;
    }

    return ret;
}


/*
 * FIFO data:
 *
 * X = 3 bytes
 * Y = 3 bytes
 * Z = 3 bytes
 *
 * 1 sample = 9 bytes
 */
uint16_t ADXL357_ConvertFifo(
        ADXL357_Handle_t *dev,
        const uint8_t *fifo_data,
        uint16_t fifo_length,
        ADXL357_Axes_t *axes,
        uint16_t max_axes)
{
    uint16_t sample_count =
        fifo_length / ADXL357_XYZ_RAW_SIZE;

    if (sample_count > max_axes)
    {
        sample_count = max_axes;
    }

    uint16_t converted = 0;

    for (uint16_t i = 0; i < sample_count; i++)
    {
        uint16_t offset =
            i * ADXL357_XYZ_RAW_SIZE;

        const uint8_t *data =
            &fifo_data[offset];

        /*
         * Python 원본의 FIFO validity 확인
         */
        if (data[2] & 0x01)
        {
            uint8_t is_empty =
                (data[2] |
                 data[5] |
                 data[8]) & 0x02;

            if (is_empty)
            {
                continue;
            }
        }
        else
        {
            break;
        }

        int32_t x =
            ADXL357_Convert20Bit(&data[0]);

        int32_t y =
            ADXL357_Convert20Bit(&data[3]);

        int32_t z =
            ADXL357_Convert20Bit(&data[6]);

        axes[converted].x =
            (float)x * dev->x_scale;

        axes[converted].y =
            (float)y * dev->y_scale;

        axes[converted].z =
            (float)z * dev->z_scale;

        converted++;
    }

    return converted;
}


/* ============================================================
 * Interrupt
 * ============================================================ */

HAL_StatusTypeDef ADXL357_EnableInterrupt(
        ADXL357_Handle_t *dev)
{
    /*
     * Python:
     *
     * REG_INT_MAP = 0x02
     */
    return ADXL357_WriteReg(
            dev,
            ADXL357_REG_INT_MAP,
            0x02);
}


HAL_StatusTypeDef ADXL357_DisableInterrupt(
        ADXL357_Handle_t *dev)
{
    return ADXL357_WriteReg(
            dev,
            ADXL357_REG_INT_MAP,
            0x00);
}


/* ============================================================
 * Status
 * ============================================================ */

HAL_StatusTypeDef ADXL357_GetStatus(
        ADXL357_Handle_t *dev,
        uint8_t *status)
{
    return ADXL357_ReadReg(
            dev,
            ADXL357_REG_STATUS,
            status);
}


uint8_t ADXL357_CheckReady(
        ADXL357_Handle_t *dev)
{
    uint8_t status;

    if (ADXL357_GetStatus(
            dev,
            &status) != HAL_OK)
    {
        return 0;
    }

    return
        (status & ADXL357_STATUS_DATA_RDY)
        ? 1U : 0U;
}


uint8_t ADXL357_CheckFifoFull(
        ADXL357_Handle_t *dev)
{
    uint8_t status;

    if (ADXL357_GetStatus(
            dev,
            &status) != HAL_OK)
    {
        return 0;
    }

    return
        (status & ADXL357_STATUS_FIFO_FULL)
        ? 1U : 0U;
}


/* ============================================================
 * XYZ
 * ============================================================ */

HAL_StatusTypeDef ADXL357_GetAxesRaw(
        ADXL357_Handle_t *dev,
        ADXL357_RawAxes_t *axes)
{
    uint8_t data[9];

    HAL_StatusTypeDef ret =
        ADXL357_ReadMulti(
                dev,
                ADXL357_REG_XDATA3,
                data,
                sizeof(data));

    if (ret != HAL_OK)
    {
        return ret;
    }

    axes->x =
        ADXL357_Convert20Bit(&data[0]);

    axes->y =
        ADXL357_Convert20Bit(&data[3]);

    axes->z =
        ADXL357_Convert20Bit(&data[6]);

    return HAL_OK;
}


HAL_StatusTypeDef ADXL357_GetAxes(
        ADXL357_Handle_t *dev,
        ADXL357_Axes_t *axes)
{
    ADXL357_RawAxes_t raw;

    HAL_StatusTypeDef ret =
        ADXL357_GetAxesRaw(
                dev,
                &raw);

    if (ret != HAL_OK)
    {
        return ret;
    }

    axes->x =
        (float)raw.x * dev->x_scale;

    axes->y =
        (float)raw.y * dev->y_scale;

    axes->z =
        (float)raw.z * dev->z_scale;

    return HAL_OK;
}


/* ============================================================
 * Temperature
 * ============================================================ */

HAL_StatusTypeDef ADXL357_GetTemperature(
        ADXL357_Handle_t *dev,
        float *temperature)
{
    uint8_t raw[2];

    HAL_StatusTypeDef ret =
        ADXL357_ReadMulti(
                dev,
                ADXL357_REG_TEMP2,
                raw,
                2);

    if (ret != HAL_OK)
    {
        return ret;
    }

    uint16_t temp_raw =
        ((uint16_t)raw[0] << 8) |
        raw[1];

    /*
     * Python:
     *
     * 25.0 + (1852 - temp) / 9.05
     */
    *temperature =
        25.0f +
        ((1852.0f - (float)temp_raw) / 9.05f);

    return HAL_OK;
}


/* ============================================================
 * Device ID
 * ============================================================ */

HAL_StatusTypeDef ADXL357_ReadDeviceID(
        ADXL357_Handle_t *dev,
        uint8_t *devid_ad,
        uint8_t *devid_mst,
        uint8_t *part_id)
{
    uint8_t data[3];

    HAL_StatusTypeDef ret =
        ADXL357_ReadMulti(
                dev,
                ADXL357_REG_DEVID_AD,
                data,
                sizeof(data));

    if (ret != HAL_OK)
    {
        return ret;
    }

    *devid_ad  = data[0];
    *devid_mst = data[1];
    *part_id   = data[2];

    return HAL_OK;
}