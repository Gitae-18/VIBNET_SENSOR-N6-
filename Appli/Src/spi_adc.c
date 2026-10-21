#include "spi_adc.h"

#include <stdio.h>

#define SPI_ADC_TIMEOUT_MS  100U
#define SPI_ADC_UART_TIMEOUT_MS 100U

typedef struct
{
    GPIO_TypeDef *sck_port;
    uint32_t sck;
    GPIO_TypeDef *mosi_port;
    uint32_t mosi;
    GPIO_TypeDef *miso_port;
    uint32_t miso;
} SPI_ADC_Pins;

static const SPI_ADC_Pins spi1_pins = {GPIOA, 5U, GPIOA, 7U, GPIOB, 8U};
static const SPI_ADC_Pins spi2_pins = {GPIOA, 9U, GPIOG, 8U, GPIOD, 11U};

static const char *SPI_ADC_Name(const SPI_ADC_HandleTypeDef *adc)
{
    if ((adc != NULL) && (adc->spi != NULL))
    {
        if (adc->spi->Instance == SPI1) return "SPI1";
        if (adc->spi->Instance == SPI2) return "SPI2";
    }
    return "SPI";
}

static void SPI_ADC_PrintPin(UART_HandleTypeDef *uart, const char *bus, const char *name,
                             GPIO_TypeDef *port, uint32_t pin)
{
    char line[144];
    int length = snprintf(line, sizeof(line),
                          "[%s DIAG] %s MODE=%lu AF=%lu PULL=%lu IDR=%lu ODR=%lu\r\n",
                          bus, name,
                          (unsigned long)((port->MODER >> (pin * 2U)) & 3U),
                          (unsigned long)((port->AFR[pin / 8U] >> ((pin % 8U) * 4U)) & 15U),
                          (unsigned long)((port->PUPDR >> (pin * 2U)) & 3U),
                          (unsigned long)((port->IDR >> pin) & 1U),
                          (unsigned long)((port->ODR >> pin) & 1U));
    if ((length > 0) && ((size_t)length < sizeof(line)))
    {
        (void)HAL_UART_Transmit(uart, (uint8_t *)line, (uint16_t)length,
                               SPI_ADC_UART_TIMEOUT_MS);
    }
}

/* Conservative >=1 us guard for CS setup / inactive time. SystemCoreClock
 * must describe the current CPU clock. Interrupts can only extend this wait.
 */
static void SPI_ADC_CS_Delay(void)
{
    volatile uint32_t cycles = (SystemCoreClock / 1000000U) + 1U;
    while (cycles != 0U)
    {
        __NOP();
        cycles--;
    }
}

/* Startup-only GPIO mode-0 comparison. MISO remains an input. */
static void SPI_ADC_GPIOCompare(const SPI_ADC_HandleTypeDef *adc,
                                UART_HandleTypeDef *uart, const SPI_ADC_Pins *pins)
{
    const uint32_t sck_mask = 3UL << (pins->sck * 2U);
    const uint32_t mosi_mask = 3UL << (pins->mosi * 2U);
    const uint32_t miso_mask = 3UL << (pins->miso * 2U);
    const uint16_t sck_pin = (uint16_t)(1UL << pins->sck);
    const uint16_t mosi_pin = (uint16_t)(1UL << pins->mosi);
    uint32_t sck_mode = pins->sck_port->MODER & sck_mask;
    uint32_t mosi_mode = pins->mosi_port->MODER & mosi_mask;
    uint32_t miso_mode = pins->miso_port->MODER & miso_mask;
    GPIO_PinState sck_out = ((pins->sck_port->ODR & sck_pin) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    GPIO_PinState mosi_out = ((pins->mosi_port->ODR & mosi_pin) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    const uint8_t tx[3] = {6U, 0U, 0U};
    uint8_t rx[3] = {0U};
    uint32_t sck_hi, sck_lo, mosi_hi, mosi_lo, i, j;
    char line[160];
    int n;
    HAL_GPIO_WritePin(adc->cs_port, adc->cs_pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(pins->sck_port, sck_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(pins->mosi_port, mosi_pin, GPIO_PIN_RESET);
    MODIFY_REG(pins->sck_port->MODER, sck_mask, 1UL << (pins->sck * 2U));
    MODIFY_REG(pins->mosi_port->MODER, mosi_mask, 1UL << (pins->mosi * 2U));
    MODIFY_REG(pins->miso_port->MODER, miso_mask, 0U);
    HAL_GPIO_WritePin(pins->sck_port, sck_pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(pins->mosi_port, mosi_pin, GPIO_PIN_SET);
    SPI_ADC_CS_Delay();
    sck_hi = (pins->sck_port->IDR >> pins->sck) & 1U;
    mosi_hi = (pins->mosi_port->IDR >> pins->mosi) & 1U;
    HAL_GPIO_WritePin(pins->sck_port, sck_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(pins->mosi_port, mosi_pin, GPIO_PIN_RESET);
    SPI_ADC_CS_Delay();
    sck_lo = (pins->sck_port->IDR >> pins->sck) & 1U;
    mosi_lo = (pins->mosi_port->IDR >> pins->mosi) & 1U;
    HAL_GPIO_WritePin(adc->cs_port, adc->cs_pin, GPIO_PIN_RESET);
    SPI_ADC_CS_Delay();
    /* Deliberately slower than hardware SPI; delay is not calibrated. */
    for (i = 0U; i < 3U; i++)
    {
        for (j = 0U; j < 8U; j++)
        {
            HAL_GPIO_WritePin(pins->mosi_port, mosi_pin,
                ((tx[i] & (0x80U >> j)) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
            SPI_ADC_CS_Delay();
            HAL_GPIO_WritePin(pins->sck_port, sck_pin, GPIO_PIN_SET);
            SPI_ADC_CS_Delay();
            rx[i] = (uint8_t)((rx[i] << 1U) | ((pins->miso_port->IDR >> pins->miso) & 1U));
            HAL_GPIO_WritePin(pins->sck_port, sck_pin, GPIO_PIN_RESET);
        }
    }
    HAL_GPIO_WritePin(adc->cs_port, adc->cs_pin, GPIO_PIN_SET);
    SPI_ADC_CS_Delay();
    HAL_GPIO_WritePin(pins->sck_port, sck_pin, sck_out);
    HAL_GPIO_WritePin(pins->mosi_port, mosi_pin, mosi_out);
    MODIFY_REG(pins->sck_port->MODER, sck_mask, sck_mode);
    MODIFY_REG(pins->mosi_port->MODER, mosi_mask, mosi_mode);
    MODIFY_REG(pins->miso_port->MODER, miso_mask, miso_mode);
    n = snprintf(line, sizeof(line),
        "[%s GPIO] SCK H=%lu L=%lu MOSI H=%lu L=%lu (expected 1,0 each)\r\n",
        SPI_ADC_Name(adc), (unsigned long)sck_hi, (unsigned long)sck_lo,
        (unsigned long)mosi_hi, (unsigned long)mosi_lo);
    if ((n > 0) && ((size_t)n < sizeof(line)))
        (void)HAL_UART_Transmit(uart, (uint8_t *)line, (uint16_t)n, SPI_ADC_UART_TIMEOUT_MS);
    n = snprintf(line, sizeof(line),
        "[%s GPIO] CH0 TX=06 00 00 RX=%02X %02X %02X NULL=%u\r\n",
        SPI_ADC_Name(adc), (unsigned int)rx[0], (unsigned int)rx[1], (unsigned int)rx[2],
        (unsigned int)((rx[1] >> 4U) & 1U));
    if ((n > 0) && ((size_t)n < sizeof(line)))
        (void)HAL_UART_Transmit(uart, (uint8_t *)line, (uint16_t)n, SPI_ADC_UART_TIMEOUT_MS);
}

void SPI_ADC_DiagnosePins(const SPI_ADC_HandleTypeDef *adc,
                          UART_HandleTypeDef *uart)
{
    const SPI_ADC_Pins *pins;
    const char *bus = SPI_ADC_Name(adc);
    uint32_t cs_index = 0U;
    char line[112];
    GPIO_PinState high_read;
    GPIO_PinState low_read;
    int length;
    if ((adc == NULL) || (adc->spi == NULL) || (adc->cs_port == NULL) ||
        (adc->cs_pin == 0U) || (uart == NULL))
    {
        return;
    }
    if (adc->spi->Instance == SPI1)
    {
        pins = &spi1_pins;
        SPI_ADC_PrintPin(uart, bus, "PA5 SCK", pins->sck_port, pins->sck);
        SPI_ADC_PrintPin(uart, bus, "PA7 MOSI", pins->mosi_port, pins->mosi);
        SPI_ADC_PrintPin(uart, bus, "PB8 MISO", pins->miso_port, pins->miso);
    }
    else if (adc->spi->Instance == SPI2)
    {
        pins = &spi2_pins;
        SPI_ADC_PrintPin(uart, bus, "PA9 SCK", pins->sck_port, pins->sck);
        SPI_ADC_PrintPin(uart, bus, "PG8 MOSI", pins->mosi_port, pins->mosi);
        SPI_ADC_PrintPin(uart, bus, "PD11 MISO", pins->miso_port, pins->miso);
    }
    else
    {
        return;
    }
    while ((adc->cs_pin & (1UL << cs_index)) == 0U) cs_index++;
    SPI_ADC_PrintPin(uart, bus, "CS", adc->cs_port, cs_index);

    /* No clock is sent: this only selects/deselects the ADC, without a
     * conversion command. It cannot verify the signal beyond the MCU pad.
     */
    HAL_GPIO_WritePin(adc->cs_port, adc->cs_pin, GPIO_PIN_SET);
    SPI_ADC_CS_Delay();
    high_read = HAL_GPIO_ReadPin(adc->cs_port, adc->cs_pin);
    HAL_GPIO_WritePin(adc->cs_port, adc->cs_pin, GPIO_PIN_RESET);
    SPI_ADC_CS_Delay();
    low_read = HAL_GPIO_ReadPin(adc->cs_port, adc->cs_pin);
    HAL_GPIO_WritePin(adc->cs_port, adc->cs_pin, GPIO_PIN_SET);
    SPI_ADC_CS_Delay();
    length = snprintf(line, sizeof(line),
                      "[%s DIAG] CS_READBACK HIGH=%u LOW=%u (expected 1,0)\r\n",
                      bus, (unsigned int)high_read, (unsigned int)low_read);
    if ((length > 0) && ((size_t)length < sizeof(line)))
    {
        (void)HAL_UART_Transmit(uart, (uint8_t *)line, (uint16_t)length,
                               SPI_ADC_UART_TIMEOUT_MS);
    }
    SPI_ADC_GPIOCompare(adc, uart, pins);
}

void SPI_ADC_CS_DiagnosticSet(const SPI_ADC_HandleTypeDef *adc,
                              UART_HandleTypeDef *uart, GPIO_PinState state)
{
    char line[144];
    int length;
    if ((adc == NULL) || (adc->cs_port == NULL) || (adc->cs_pin == 0U) ||
        (uart == NULL))
    {
        return;
    }
    HAL_GPIO_WritePin(adc->cs_port, adc->cs_pin, state);
    SPI_ADC_CS_Delay();
    length = snprintf(line, sizeof(line),
        "[%s CS TEST] CS=%s CS_READ=%u HOLD=%us ADC_READ=OFF\r\n",
        SPI_ADC_Name(adc), (state == GPIO_PIN_SET) ? "HIGH" : "LOW",
        (unsigned int)HAL_GPIO_ReadPin(adc->cs_port, adc->cs_pin),
        (unsigned int)SPI_ADC_CS_DIAG_HOLD_SECONDS);
    if ((length > 0) && ((size_t)length < sizeof(line)))
    {
        (void)HAL_UART_Transmit(uart, (uint8_t *)line, (uint16_t)length,
                               SPI_ADC_UART_TIMEOUT_MS);
    }
}

static HAL_StatusTypeDef SPI_ADC_ReadFrame(const SPI_ADC_HandleTypeDef *adc,
                                          uint8_t channel, uint16_t *raw,
                                          uint8_t rx[3], uint8_t *invalid_frame)
{
    uint8_t tx[3];
    HAL_StatusTypeDef status;

    *invalid_frame = 0U;

    if ((adc == NULL) || (adc->spi == NULL) || (adc->cs_port == NULL) ||
        (adc->cs_pin == 0U) || (raw == NULL) ||
        (channel >= SPI_ADC_CHANNEL_COUNT))
    {
        return HAL_ERROR;
    }

    /* Start=1, single-ended=1, channel D2:D0, followed by dummy clocks. */
    tx[0] = (uint8_t)(0x06U | (channel >> 2U));
    tx[1] = (uint8_t)((channel & 0x03U) << 6U);
    tx[2] = 0U;

    HAL_GPIO_WritePin(adc->cs_port, adc->cs_pin, GPIO_PIN_SET);
    SPI_ADC_CS_Delay();
    HAL_GPIO_WritePin(adc->cs_port, adc->cs_pin, GPIO_PIN_RESET);
    SPI_ADC_CS_Delay();
    status = HAL_SPI_TransmitReceive(adc->spi, tx, rx, 3U, SPI_ADC_TIMEOUT_MS);
    /* Release CS even on timeout/error; leave the caller's value unchanged. */
    HAL_GPIO_WritePin(adc->cs_port, adc->cs_pin, GPIO_PIN_SET);
    if (status == HAL_OK)
    {
        /* Figure 6-1: byte 2 bit 4 must be zero. A zero alone does not
         * establish that an ADC is connected (e.g. stuck-low MISO).
         */
        if ((rx[1] & 0x10U) != 0U)
        {
            *invalid_frame = 1U;
            return HAL_ERROR;
        }
        *raw = (uint16_t)(((uint16_t)(rx[1] & 0x0FU) << 8U) | rx[2]);
    }
    return status;
}

HAL_StatusTypeDef SPI_ADC_ReadChannel(const SPI_ADC_HandleTypeDef *adc,
                                     uint8_t channel, uint16_t *raw)
{
    uint8_t rx[3] = {0U};
    uint8_t invalid_frame;
    return SPI_ADC_ReadFrame(adc, channel, raw, rx, &invalid_frame);
}

HAL_StatusTypeDef SPI_ADC_PrintAll(const SPI_ADC_HandleTypeDef *adc,
                                  UART_HandleTypeDef *uart)
{
    HAL_StatusTypeDef result = HAL_OK;
    uint8_t channel;
    if ((adc == NULL) || (uart == NULL))
    {
        return HAL_ERROR;
    }
    for (channel = 0U; channel < SPI_ADC_CHANNEL_COUNT; channel++)
    {
        char line[160];
        uint8_t rx[3] = {0U};
        uint8_t invalid_frame;
        uint16_t raw = 0U;
        int length;
        HAL_StatusTypeDef status = SPI_ADC_ReadFrame(adc, channel, &raw, rx, &invalid_frame);
        if (status == HAL_OK)
        {
            uint32_t mv = ((uint32_t)raw * SPI_ADC_VREF_MV + 2048U) / 4096U;
            length = snprintf(line, sizeof(line),
                              "[%s ADC] CH%u RAW=%u VOLT=%lu mV RX=%02X %02X %02X NULL=0\r\n",
                              SPI_ADC_Name(adc), (unsigned int)channel, (unsigned int)raw,
                              (unsigned long)mv,
                              (unsigned int)rx[0], (unsigned int)rx[1],
                              (unsigned int)rx[2]);
        }
        else if (invalid_frame != 0U)
        {
            result = status;
            length = snprintf(line, sizeof(line),
                              "[%s ADC] CH%u INVALID_FRAME RX=%02X %02X %02X NULL=1 (expected 0)\r\n",
                              SPI_ADC_Name(adc), (unsigned int)channel, (unsigned int)rx[0],
                              (unsigned int)rx[1], (unsigned int)rx[2]);
        }
        else
        {
            result = status;
            length = snprintf(line, sizeof(line),
                              "[%s ADC] CH%u READ ERROR status=%u\r\n",
                              SPI_ADC_Name(adc), (unsigned int)channel, (unsigned int)status);
        }
        if ((length < 0) || ((size_t)length >= sizeof(line)))
        {
            return HAL_ERROR;
        }
        status = HAL_UART_Transmit(uart, (uint8_t *)line, (uint16_t)length,
                                   SPI_ADC_UART_TIMEOUT_MS);
        if (status != HAL_OK)
        {
            return status;
        }
    }
    return result;
}
