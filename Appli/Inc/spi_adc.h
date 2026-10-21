#ifndef SPI_ADC_H
#define SPI_ADC_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPI_ADC_CHANNEL_COUNT 8U
#define SPI_ADC_VREF_MV       5000U
/* 1: hold CS HIGH/LOW for multimeter inspection; no ADC sampling.
 * Set to 0 and rebuild to restore the ADC monitor.
 */
#define SPI_ADC_CS_DIAG_ENABLE 0U
#define SPI_ADC_CS_DIAG_HOLD_SECONDS 3U

/* MCP3208: initialized SPI master, mode 0, 8 bits, MSB first, software NSS.
 * CS must already be configured as a push-pull output, initially HIGH.
 * Serialize access: call these blocking functions from one task only.
 */
typedef struct
{
    SPI_HandleTypeDef *spi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
} SPI_ADC_HandleTypeDef;

/* Returns HAL_ERROR on a nonzero NULL bit; leaves *raw unchanged on failure. */
HAL_StatusTypeDef SPI_ADC_ReadChannel(const SPI_ADC_HandleTypeDef *adc,
                                     uint8_t channel, uint16_t *raw);
/* Print raw/millivolts only after a completed transfer with NULL=0.
 * Report transport and frame errors separately, without measurement values.
 * HAL_OK does not guarantee detection of an attached ADC.
 */
HAL_StatusTypeDef SPI_ADC_PrintAll(const SPI_ADC_HandleTypeDef *adc,
                                  UART_HandleTypeDef *uart);
/* Board-specific, one-shot MCU pin/readback diagnostic. Call before sampling.
 * SPI1: PA5 SCK, PA7 MOSI, PB8 MISO, PF4 CS.
 * SPI2: PA9 SCK, PG8 MOSI, PD11 MISO, PC1 CS.
 * The ADC handle selects the SPI bus and software CS used by all diagnostics.
 */
void SPI_ADC_DiagnosePins(const SPI_ADC_HandleTypeDef *adc,
                          UART_HandleTypeDef *uart);
/* Sets CS and reports MCU pad readback; caller holds the state using RTOS sleep. */
void SPI_ADC_CS_DiagnosticSet(const SPI_ADC_HandleTypeDef *adc,
                              UART_HandleTypeDef *uart, GPIO_PinState state);

#ifdef __cplusplus
}
#endif
#endif
