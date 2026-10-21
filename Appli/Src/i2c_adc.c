#include "i2c_adc.h"

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* Hardware configuration                                                     */
/* -------------------------------------------------------------------------- */

#define TCA9548A_ADDR_MIN              0x70U
#define TCA9548A_ADDR_MAX              0x77U

#define TCA9548A_ADS1015_CHANNEL       4U
#define TCA9548A_ADS1015_CHANNEL_MASK  0x10U

#define ADS1015_ADDR                   0x48U

#define ADS1015_REG_CONVERSION         0x00U
#define ADS1015_REG_CONFIG             0x01U

#define ADS1015_CONFIG_RESET_VALUE     0x8583U
#define ADS1015_CONV_RESET_VALUE       0x0000U

#define I2C_READY_TRIALS               3U
#define I2C_TIMEOUT_MS                 100U

#define I2C_SCAN_ADDR_MIN              0x08U
#define I2C_SCAN_ADDR_MAX              0x77U

#define I2C_HAL_ADDR(addr7)            ((uint16_t)((addr7) << 1U))

/*
 * Board schematic:
 * I2C2_RST = PE4
 * TCA9548A RESET is active-low.
 */
#define TCA_RST_GPIO_PORT              I2C2_RST_GPIO_Port
#define TCA_RST_GPIO_PIN               I2C2_RST_Pin

/* I2C2 is physically routed to PB10 (SCL) and PD4 (SDA). */
#define I2C2_SCL_GPIO_PORT             GPIOB
#define I2C2_SCL_GPIO_PIN              GPIO_PIN_10
#define I2C2_SDA_GPIO_PORT             GPIOD
#define I2C2_SDA_GPIO_PIN              GPIO_PIN_4

/* -------------------------------------------------------------------------- */
/* UART debug                                                                 */
/* -------------------------------------------------------------------------- */

static void I2C_ADC_Print(UART_HandleTypeDef *huart, const char *text)
{
    if ((huart == NULL) || (text == NULL))
    {
        return;
    }

    (void)HAL_UART_Transmit(huart,
                           (uint8_t *)text,
                           (uint16_t)strlen(text),
                           HAL_MAX_DELAY);
}

static void I2C_ADC_PrintHex(UART_HandleTypeDef *huart,
                             const char *prefix,
                             uint32_t value,
                             uint32_t width)
{
    char buf[96];

    if (width == 2U)
    {
        (void)snprintf(buf, sizeof(buf),
                       "%s0x%02lX\r\n",
                       prefix,
                       (unsigned long)value);
    }
    else if (width == 4U)
    {
        (void)snprintf(buf, sizeof(buf),
                       "%s0x%04lX\r\n",
                       prefix,
                       (unsigned long)value);
    }
    else
    {
        (void)snprintf(buf, sizeof(buf),
                       "%s0x%08lX\r\n",
                       prefix,
                       (unsigned long)value);
    }

    I2C_ADC_Print(huart, buf);
}

static const char *I2C_ADC_StatusText(HAL_StatusTypeDef status)
{
    switch (status)
    {
        case HAL_OK:
            return "OK";

        case HAL_ERROR:
            return "ERROR";

        case HAL_BUSY:
            return "BUSY";

        case HAL_TIMEOUT:
            return "TIMEOUT";

        default:
            return "UNKNOWN";
    }
}

static void I2C_ADC_PrintBusLevels(UART_HandleTypeDef *huart)
{
    char buf[96];
    uint32_t reset_level;
    uint32_t scl_level;
    uint32_t sda_level;

    reset_level = (HAL_GPIO_ReadPin(TCA_RST_GPIO_PORT,
                                    TCA_RST_GPIO_PIN) == GPIO_PIN_SET) ? 1U : 0U;
    scl_level = (HAL_GPIO_ReadPin(I2C2_SCL_GPIO_PORT,
                                  I2C2_SCL_GPIO_PIN) == GPIO_PIN_SET) ? 1U : 0U;
    sda_level = (HAL_GPIO_ReadPin(I2C2_SDA_GPIO_PORT,
                                  I2C2_SDA_GPIO_PIN) == GPIO_PIN_SET) ? 1U : 0U;

    (void)snprintf(buf, sizeof(buf),
                   "[GPIO] RESET(PE4)=%lu SCL(PB10)=%lu SDA(PD4)=%lu\r\n",
                   (unsigned long)reset_level,
                   (unsigned long)scl_level,
                   (unsigned long)sda_level);
    I2C_ADC_Print(huart, buf);
}

static void I2C_ADC_PrintRegisters(I2C_HandleTypeDef *hi2c,
                                   UART_HandleTypeDef *huart,
                                   const char *title)
{
    uint32_t cr1;
    uint32_t isr;
    char buf[160];

    if ((hi2c == NULL) || (hi2c->Instance == NULL) ||
        (huart == NULL) || (title == NULL))
    {
        return;
    }

    cr1 = hi2c->Instance->CR1;
    isr = hi2c->Instance->ISR;

    I2C_ADC_Print(huart, title);
    I2C_ADC_PrintHex(huart, "  CR1     = ", cr1, 8U);
    I2C_ADC_PrintHex(huart, "  CR2     = ", hi2c->Instance->CR2, 8U);
    I2C_ADC_PrintHex(huart, "  TIMINGR = ", hi2c->Instance->TIMINGR, 8U);
    I2C_ADC_PrintHex(huart, "  TIMEOUTR= ", hi2c->Instance->TIMEOUTR, 8U);
    I2C_ADC_PrintHex(huart, "  ISR     = ", isr, 8U);

    (void)snprintf(buf, sizeof(buf),
                   "  FLAGS1: PE=%lu BUSY=%lu TXIS=%lu RXNE=%lu TC=%lu TCR=%lu\r\n",
                   (unsigned long)((cr1 & I2C_CR1_PE) != 0U),
                   (unsigned long)((isr & I2C_ISR_BUSY) != 0U),
                   (unsigned long)((isr & I2C_ISR_TXIS) != 0U),
                   (unsigned long)((isr & I2C_ISR_RXNE) != 0U),
                   (unsigned long)((isr & I2C_ISR_TC) != 0U),
                   (unsigned long)((isr & I2C_ISR_TCR) != 0U));
    I2C_ADC_Print(huart, buf);

    (void)snprintf(buf, sizeof(buf),
                   "  FLAGS2: NACK=%lu STOP=%lu BERR=%lu ARLO=%lu OVR=%lu TIMEOUT=%lu\r\n",
                   (unsigned long)((isr & I2C_ISR_NACKF) != 0U),
                   (unsigned long)((isr & I2C_ISR_STOPF) != 0U),
                   (unsigned long)((isr & I2C_ISR_BERR) != 0U),
                   (unsigned long)((isr & I2C_ISR_ARLO) != 0U),
                   (unsigned long)((isr & I2C_ISR_OVR) != 0U),
                   (unsigned long)((isr & I2C_ISR_TIMEOUT) != 0U));
    I2C_ADC_Print(huart, buf);
}

static void I2C_ADC_PrintProbeResult(UART_HandleTypeDef *huart,
                                     I2C_HandleTypeDef *hi2c,
                                     uint8_t addr,
                                     HAL_StatusTypeDef status,
                                     const char *prefix)
{
    char buf[128];

    (void)snprintf(buf, sizeof(buf),
                   "%s0x%02X: %s STATE=0x%02lX ERROR=0x%08lX\r\n",
                   prefix,
                   (unsigned int)addr,
                   I2C_ADC_StatusText(status),
                   (unsigned long)HAL_I2C_GetState(hi2c),
                   (unsigned long)HAL_I2C_GetError(hi2c));
    I2C_ADC_Print(huart, buf);
}

static void I2C_ADC_FullBusScan(I2C_HandleTypeDef *hi2c,
                                UART_HandleTypeDef *huart)
{
    uint8_t addr;
    uint32_t ack_count = 0U;
    HAL_StatusTypeDef status;
    char buf[80];

    I2C_ADC_Print(huart, "[FULL SCAN] 0x08-0x77 START\r\n");

    for (addr = I2C_SCAN_ADDR_MIN; addr <= I2C_SCAN_ADDR_MAX; addr++)
    {
        status = HAL_I2C_IsDeviceReady(hi2c,
                                       I2C_HAL_ADDR(addr),
                                       1U,
                                       I2C_TIMEOUT_MS);

        if (status == HAL_OK)
        {
            ack_count++;
            I2C_ADC_PrintProbeResult(huart,
                                     hi2c,
                                     addr,
                                     status,
                                     "[FULL SCAN] ACK ");
        }
    }

    (void)snprintf(buf, sizeof(buf),
                   "[FULL SCAN] DONE, ACK COUNT=%lu\r\n",
                   (unsigned long)ack_count);
    I2C_ADC_Print(huart, buf);

    if (ack_count == 0U)
    {
        I2C_ADC_Print(huart,
                      "[FULL SCAN] No I2C device responded\r\n");
    }
}

/* -------------------------------------------------------------------------- */
/* TCA9548A-Q1                                                                */
/* -------------------------------------------------------------------------- */

static void TCA9548A_HardwareReset(void)
{
    /*
     * MX_GPIO_Init() drives PE4 high immediately to enable the external
     * circuit.  Keep the requested stabilization delay, then assert HIGH
     * again before accessing the TCA9548A.
     */
    HAL_Delay(100U);

    HAL_GPIO_WritePin(TCA_RST_GPIO_PORT,
                      TCA_RST_GPIO_PIN,
                      GPIO_PIN_SET);

    HAL_Delay(10U);
}

static HAL_StatusTypeDef TCA9548A_Find(I2C_HandleTypeDef *hi2c,
                                       UART_HandleTypeDef *huart,
                                       uint8_t *found_addr)
{
    uint8_t addr;
    HAL_StatusTypeDef status;

    if ((hi2c == NULL) || (huart == NULL) || (found_addr == NULL))
    {
        return HAL_ERROR;
    }

    for (addr = TCA9548A_ADDR_MIN; addr <= TCA9548A_ADDR_MAX; addr++)
    {
        status = HAL_I2C_IsDeviceReady(hi2c,
                                       I2C_HAL_ADDR(addr),
                                       I2C_READY_TRIALS,
                                       I2C_TIMEOUT_MS);

        I2C_ADC_PrintProbeResult(huart,
                                 hi2c,
                                 addr,
                                 status,
                                 "[TCA SCAN] ");

        if (status == HAL_OK)
        {
            *found_addr = addr;
            return HAL_OK;
        }
    }

    return HAL_ERROR;
}

static HAL_StatusTypeDef TCA9548A_SelectChannel(I2C_HandleTypeDef *hi2c,
                                                 uint8_t tca_addr,
                                                 uint8_t channel)
{
    uint8_t control;

    if ((hi2c == NULL) || (channel > 7U))
    {
        return HAL_ERROR;
    }

    control = (uint8_t)(1U << channel);

    return HAL_I2C_Master_Transmit(hi2c,
                                   I2C_HAL_ADDR(tca_addr),
                                   &control,
                                   1U,
                                   I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef TCA9548A_ReadControl(I2C_HandleTypeDef *hi2c,
                                               uint8_t tca_addr,
                                               uint8_t *control)
{
    if ((hi2c == NULL) || (control == NULL))
    {
        return HAL_ERROR;
    }

    return HAL_I2C_Master_Receive(hi2c,
                                  I2C_HAL_ADDR(tca_addr),
                                  control,
                                  1U,
                                  I2C_TIMEOUT_MS);
}

/* -------------------------------------------------------------------------- */
/* ADS1015-Q1                                                                 */
/* -------------------------------------------------------------------------- */

static HAL_StatusTypeDef ADS1015_Check(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == NULL)
    {
        return HAL_ERROR;
    }

    return HAL_I2C_IsDeviceReady(hi2c,
                                 I2C_HAL_ADDR(ADS1015_ADDR),
                                 I2C_READY_TRIALS,
                                 I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef ADS1015_ReadRegister(I2C_HandleTypeDef *hi2c,
                                               uint8_t reg,
                                               uint16_t *value)
{
    uint8_t rx[2];

    if ((hi2c == NULL) || (value == NULL))
    {
        return HAL_ERROR;
    }

    /*
     * ADS1015 register read sequence:
     * 1) write Address Pointer register
     * 2) read 16-bit register value
     */
    if (HAL_I2C_Master_Transmit(hi2c,
                               I2C_HAL_ADDR(ADS1015_ADDR),
                               &reg,
                               1U,
                               I2C_TIMEOUT_MS) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (HAL_I2C_Master_Receive(hi2c,
                              I2C_HAL_ADDR(ADS1015_ADDR),
                              rx,
                              2U,
                              I2C_TIMEOUT_MS) != HAL_OK)
    {
        return HAL_ERROR;
    }

    *value = ((uint16_t)rx[0] << 8U) | (uint16_t)rx[1];

    return HAL_OK;
}

/* -------------------------------------------------------------------------- */
/* Public test                                                                */
/* -------------------------------------------------------------------------- */

void I2C_ADC_Test(I2C_HandleTypeDef *hi2c, UART_HandleTypeDef *huart)
{
    uint8_t tca_addr = 0U;
    uint8_t tca_control = 0U;
    uint16_t ads_config = 0U;
    uint16_t ads_conversion = 0U;
    uint8_t warning = 0U;

    I2C_ADC_Print(huart,
                  "\r\n"
                  "=== TCA9548A / ADS1015 I2C TEST ===\r\n");

    if ((hi2c == NULL) || (huart == NULL))
    {
        return;
    }

    /*
     * 1. Hardware reset TCA9548A.
     */
    TCA9548A_HardwareReset();
    I2C_ADC_Print(huart, "[TCA] RESET OK (PE4)\r\n");

    /*
     * Diagnostic step 1: verify the levels seen by the MCU after reset release.
     * An idle I2C bus should normally report RESET=1, SCL=1 and SDA=1.
     */
    I2C_ADC_PrintBusLevels(huart);
    I2C_ADC_PrintRegisters(hi2c,
                           huart,
                           "[I2C2 REG BEFORE TCA SCAN]\r\n");

    /*
     * Diagnostic step 2: detect TCA address and print the HAL result for
     * every possible TCA9548A address.
     *    A0/A1/A2 DIP selection means address can be 0x70~0x77.
     */
    if (TCA9548A_Find(hi2c, huart, &tca_addr) != HAL_OK)
    {
        I2C_ADC_Print(huart, "[TCA] NOT FOUND (0x70~0x77)\r\n");
        I2C_ADC_PrintRegisters(hi2c,
                               huart,
                               "[I2C2 REG AFTER TCA SCAN]\r\n");

        /*
         * Diagnostic step 3: scan every legal 7-bit device address.  Only
         * ACK responses are printed so the UART log remains readable.
         */
        I2C_ADC_FullBusScan(hi2c, huart);
        I2C_ADC_PrintRegisters(hi2c,
                               huart,
                               "[I2C2 REG AFTER FULL SCAN]\r\n");
        I2C_ADC_Print(huart, "[TEST] FAIL\r\n");
        return;
    }

    I2C_ADC_PrintHex(huart,
                     "[TCA] Found at ",
                     tca_addr,
                     2U);

    /*
     * 3. Open TCA channel 4.
     *    CH4 -> control byte 0x10.
     */
    if (TCA9548A_SelectChannel(hi2c,
                              tca_addr,
                              TCA9548A_ADS1015_CHANNEL) != HAL_OK)
    {
        I2C_ADC_Print(huart, "[TCA] CH4 SELECT FAIL\r\n");
        I2C_ADC_Print(huart, "[TEST] FAIL\r\n");
        return;
    }

    I2C_ADC_Print(huart, "[TCA] CH4 Select OK\r\n");

    HAL_Delay(2U);

    /*
     * 4. Read back TCA control register.
     */
    if (TCA9548A_ReadControl(hi2c,
                             tca_addr,
                             &tca_control) != HAL_OK)
    {
        I2C_ADC_Print(huart, "[TCA] CTRL READ FAIL\r\n");
        I2C_ADC_Print(huart, "[TEST] FAIL\r\n");
        return;
    }

    I2C_ADC_PrintHex(huart,
                     "[TCA] CTRL = ",
                     tca_control,
                     2U);

    if (tca_control != TCA9548A_ADS1015_CHANNEL_MASK)
    {
        warning = 1U;
        I2C_ADC_Print(huart,
                      "[TCA] WARNING: expected CTRL = 0x10\r\n");
    }

    /*
     * 5. ADS1015 should now be visible behind CH4 at 0x48.
     */
    if (ADS1015_Check(hi2c) != HAL_OK)
    {
        I2C_ADC_Print(huart, "[ADS1015] NOT FOUND at 0x48\r\n");
        I2C_ADC_Print(huart, "[TEST] FAIL\r\n");
        return;
    }

    I2C_ADC_Print(huart, "[ADS1015] Found at 0x48\r\n");

    /*
     * 6. Read Config register.
     *    Reset/default value = 0x8583.
     */
    if (ADS1015_ReadRegister(hi2c,
                             ADS1015_REG_CONFIG,
                             &ads_config) != HAL_OK)
    {
        I2C_ADC_Print(huart, "[ADS1015] CONFIG READ FAIL\r\n");
        I2C_ADC_Print(huart, "[TEST] FAIL\r\n");
        return;
    }

    I2C_ADC_PrintHex(huart,
                     "[ADS1015] CONFIG = ",
                     ads_config,
                     4U);

    if (ads_config != ADS1015_CONFIG_RESET_VALUE)
    {
        warning = 1U;
        I2C_ADC_Print(huart,
                      "[ADS1015] WARNING: expected CONFIG = 0x8583\r\n");
    }

    /*
     * 7. Read Conversion register.
     *    Immediately after reset and before the first conversion,
     *    the expected value is 0x0000.
     */
    if (ADS1015_ReadRegister(hi2c,
                             ADS1015_REG_CONVERSION,
                             &ads_conversion) != HAL_OK)
    {
        I2C_ADC_Print(huart, "[ADS1015] CONVERSION READ FAIL\r\n");
        I2C_ADC_Print(huart, "[TEST] FAIL\r\n");
        return;
    }

    I2C_ADC_PrintHex(huart,
                     "[ADS1015] CONVERSION = ",
                     ads_conversion,
                     4U);

    if (ads_conversion != ADS1015_CONV_RESET_VALUE)
    {
        warning = 1U;
        I2C_ADC_Print(huart,
                      "[ADS1015] WARNING: expected CONVERSION = 0x0000\r\n");
    }

    if (warning == 0U)
    {
        I2C_ADC_Print(huart, "[TEST] PASS\r\n");
    }
    else
    {
        I2C_ADC_Print(huart, "[TEST] PASS WITH WARNING\r\n");
    }
}
