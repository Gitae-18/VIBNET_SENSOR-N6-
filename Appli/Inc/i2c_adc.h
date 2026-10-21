#ifndef __I2C_ADC_H
#define __I2C_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/*
 * Hardware path on SCS-BASE-BD-V800:
 *
 * STM32 I2C2
 *   -> TCA9548A-Q1
 *   -> TCA channel 4
 *   -> ADS1015-Q1 (7-bit address 0x48)
 *
 * Call I2C_ADC_Test() once after MX_I2C2_Init() and UART init.
 */

void I2C_ADC_Test(I2C_HandleTypeDef *hi2c, UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif /* __I2C_ADC_H */
