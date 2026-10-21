/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.h
  * @author  MCD Application Team
  * @brief   ThreadX applicative header file
  ******************************************************************************
    * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __APP_THREADX_H
#define __APP_THREADX_H
#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "tx_api.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "sensor_task.h"
#include "vibnet_protocol.h"

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Main thread defines -------------------------------------------------------*/
/* USER CODE BEGIN MTD */

/* USER CODE END MTD */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
UINT App_ThreadX_Init(VOID *memory_ptr);
void MX_ThreadX_Init(void);

/* USER CODE BEGIN EFP */
HAL_StatusTypeDef VibnetSensor_Start(void);
HAL_StatusTypeDef VibnetSensor_Stop(void);
HAL_StatusTypeDef VibnetSensor_ApplyConfig(
        const SensorTask_Config_t *config);
HAL_StatusTypeDef VibnetSensor_SetRangeG(uint16_t range_g);
HAL_StatusTypeDef VibnetSensor_SetFilter(
        ADXL357_HPF_t hpf,
        ADXL357_ODR_t odr);
HAL_StatusTypeDef VibnetSensor_SetSensitivity(
        float x_sensitivity,
        float y_sensitivity,
        float z_sensitivity);
HAL_StatusTypeDef VibnetSensor_GetConfig(
        SensorTask_Config_t *config);
HAL_StatusTypeDef VibnetSensor_GetLatest(
        ADXL357_Axes_t *axes);
void VibnetSensor_GetStreamStats(VibnetStreamStats_t *stats);

/* USER CODE END EFP */

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

#ifdef __cplusplus
}
#endif
#endif /* __APP_THREADX_H */
