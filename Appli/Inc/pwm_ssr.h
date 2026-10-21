#ifndef __PWM_SSR_H
#define __PWM_SSR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define PWM_CONTROL_MIN_VALUE  0U
#define PWM_CONTROL_MAX_VALUE  1000U

typedef enum
{
  PWM_OUTPUT_TIM4 = 0,
  PWM_OUTPUT_TIM8,
  PWM_OUTPUT_TIM14,
  PWM_OUTPUT_TIM15,
  PWM_OUTPUT_COUNT
} PWM_Output_t;

typedef enum
{
  SSR_OUTPUT_1 = 0,
  SSR_OUTPUT_2,
  SSR_OUTPUT_3,
  SSR_OUTPUT_4,
  SSR_OUTPUT_5,
  SSR_OUTPUT_COUNT
} SSR_Output_t;

HAL_StatusTypeDef PWM_Control_Init(TIM_HandleTypeDef *htim4,
                                   TIM_HandleTypeDef *htim8,
                                   TIM_HandleTypeDef *htim14,
                                   TIM_HandleTypeDef *htim15);
HAL_StatusTypeDef PWM_Control_Set(PWM_Output_t output, uint16_t value);
uint16_t PWM_Control_Get(PWM_Output_t output);
void PWM_Control_AllOff(void);

void SSR_Control_Init(void);
HAL_StatusTypeDef SSR_Control_Set(SSR_Output_t output, uint8_t on);
GPIO_PinState SSR_Control_Get(SSR_Output_t output);
void SSR_Control_AllOff(void);

#ifdef __cplusplus
}
#endif

#endif /* __PWM_SSR_H */
