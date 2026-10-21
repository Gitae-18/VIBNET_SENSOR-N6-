#ifndef __PSRAM_TEST_H
#define __PSRAM_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define PSRAM_TEST_BASE_ADDRESS  0x90000000UL
#define PSRAM_TEST_SIZE_BYTES    (32UL * 1024UL * 1024UL)

HAL_StatusTypeDef PSRAM_FullTest(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif /* __PSRAM_TEST_H */
