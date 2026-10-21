/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined ( __ICCARM__ )
#  define CMSE_NS_CALL  __cmse_nonsecure_call
#  define CMSE_NS_ENTRY __cmse_nonsecure_entry
#else
#  define CMSE_NS_CALL  __attribute((cmse_nonsecure_call))
#  define CMSE_NS_ENTRY __attribute((cmse_nonsecure_entry))
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32n6xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* Function pointer declaration in non-secure*/
#if defined ( __ICCARM__ )
typedef void (CMSE_NS_CALL *funcptr)(void);
#else
typedef void CMSE_NS_CALL (*funcptr)(void);
#endif

/* typedef for non-secure callback functions */
typedef funcptr funcptr_NS;

/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

void App_USB2_PCD_Init(void);
void App_Diag_FatalBlink(uint32_t code);
void LED_CTRL_02_Init(void);
void LED_CTRL_02_Set(uint32_t on);
void LED_CTRL_02_On(void);
void LED_CTRL_02_Off(void);
void LED_CTRL_02_Toggle(void);
extern volatile uint32_t g_app_diag_stage;
extern volatile uint32_t g_usb2_irq_seen;
extern volatile uint32_t g_usb2_reset_seen;
extern volatile uint32_t g_usb2_enum_done_seen;

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define TIM08_CH1_Pin GPIO_PIN_3
#define TIM08_CH1_GPIO_Port GPIOQ
#define TIM15_CH1_Pin GPIO_PIN_6
#define TIM15_CH1_GPIO_Port GPIOB
#define TIM14_CH1_Pin GPIO_PIN_2
#define TIM14_CH1_GPIO_Port GPIOG
#define TIM04_CH1_Pin GPIO_PIN_13
#define TIM04_CH1_GPIO_Port GPIOG

/* USER CODE BEGIN Private defines */

#ifndef ADC1_CS_Pin
#define ADC1_CS_Pin           GPIO_PIN_4
#define ADC1_CS_GPIO_Port     GPIOF
#endif

#ifndef SPI2_CS_Pin
#define SPI2_CS_Pin           GPIO_PIN_1
#define SPI2_CS_GPIO_Port     GPIOC
#endif

#ifndef I2C2_RST_Pin
#define I2C2_RST_Pin            GPIO_PIN_4
#define I2C2_RST_GPIO_Port      GPIOE
#endif

#define LED_CTRL_02_Pin       GPIO_PIN_9
#define LED_CTRL_02_GPIO_Port GPIOH

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
