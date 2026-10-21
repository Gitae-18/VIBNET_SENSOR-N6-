#include "pwm_ssr.h"

typedef struct
{
  GPIO_TypeDef *port;
  uint16_t pin;
} SSR_Pin_t;

/* Schematic mapping: GPIO_3V3_OUT1..5 -> SSR1..5. */
static const SSR_Pin_t s_ssr_pins[SSR_OUTPUT_COUNT] =
{
  {GPIOB, GPIO_PIN_9},
  {GPIOD, GPIO_PIN_8},
  {GPIOB, GPIO_PIN_0},
  {GPIOE, GPIO_PIN_3},
  {GPIOE, GPIO_PIN_2}
};

static TIM_HandleTypeDef *s_pwm_handles[PWM_OUTPUT_COUNT];

HAL_StatusTypeDef PWM_Control_Init(TIM_HandleTypeDef *htim4,
                                   TIM_HandleTypeDef *htim8,
                                   TIM_HandleTypeDef *htim14,
                                   TIM_HandleTypeDef *htim15)
{
  uint32_t index;

  if ((htim4 == NULL) || (htim8 == NULL) ||
      (htim14 == NULL) || (htim15 == NULL))
  {
    return HAL_ERROR;
  }

  s_pwm_handles[PWM_OUTPUT_TIM4] = htim4;
  s_pwm_handles[PWM_OUTPUT_TIM8] = htim8;
  s_pwm_handles[PWM_OUTPUT_TIM14] = htim14;
  s_pwm_handles[PWM_OUTPUT_TIM15] = htim15;

  /* Start every channel at 0/1000 (always LOW). */
  for (index = 0U; index < (uint32_t)PWM_OUTPUT_COUNT; index++)
  {
    __HAL_TIM_SET_COMPARE(s_pwm_handles[index], TIM_CHANNEL_1, 0U);
    if (HAL_TIM_PWM_Start(s_pwm_handles[index], TIM_CHANNEL_1) != HAL_OK)
    {
      PWM_Control_AllOff();
      return HAL_ERROR;
    }
  }

  return HAL_OK;
}

HAL_StatusTypeDef PWM_Control_Set(PWM_Output_t output, uint16_t value)
{
  if (((uint32_t)output >= (uint32_t)PWM_OUTPUT_COUNT) ||
      (value > PWM_CONTROL_MAX_VALUE) ||
      (s_pwm_handles[output] == NULL))
  {
    return HAL_ERROR;
  }

  /* ARR=999: CCR=0 is always LOW, CCR=1000 is always HIGH. */
  __HAL_TIM_SET_COMPARE(s_pwm_handles[output], TIM_CHANNEL_1, value);
  return HAL_OK;
}

uint16_t PWM_Control_Get(PWM_Output_t output)
{
  if (((uint32_t)output >= (uint32_t)PWM_OUTPUT_COUNT) ||
      (s_pwm_handles[output] == NULL))
  {
    return 0U;
  }

  return (uint16_t)__HAL_TIM_GET_COMPARE(s_pwm_handles[output], TIM_CHANNEL_1);
}

void PWM_Control_AllOff(void)
{
  uint32_t index;

  for (index = 0U; index < (uint32_t)PWM_OUTPUT_COUNT; index++)
  {
    if (s_pwm_handles[index] != NULL)
    {
      __HAL_TIM_SET_COMPARE(s_pwm_handles[index], TIM_CHANNEL_1, 0U);
    }
  }
}

void SSR_Control_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  /* Preload LOW before changing the pins to outputs: all SSRs start OFF. */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_9, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_RESET);

  HAL_GPIO_ConfigPinAttributes(GPIOB, GPIO_PIN_0 | GPIO_PIN_9,
                               GPIO_PIN_SEC | GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOD, GPIO_PIN_8,
                               GPIO_PIN_SEC | GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE, GPIO_PIN_2 | GPIO_PIN_3,
                               GPIO_PIN_SEC | GPIO_PIN_NPRIV);

  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;

  gpio.Pin = GPIO_PIN_0 | GPIO_PIN_9;
  HAL_GPIO_Init(GPIOB, &gpio);

  gpio.Pin = GPIO_PIN_8;
  HAL_GPIO_Init(GPIOD, &gpio);

  gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
  HAL_GPIO_Init(GPIOE, &gpio);
}

HAL_StatusTypeDef SSR_Control_Set(SSR_Output_t output, uint8_t on)
{
  if ((uint32_t)output >= (uint32_t)SSR_OUTPUT_COUNT)
  {
    return HAL_ERROR;
  }

  HAL_GPIO_WritePin(s_ssr_pins[output].port,
                    s_ssr_pins[output].pin,
                    (on != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  return HAL_OK;
}

GPIO_PinState SSR_Control_Get(SSR_Output_t output)
{
  if ((uint32_t)output >= (uint32_t)SSR_OUTPUT_COUNT)
  {
    return GPIO_PIN_RESET;
  }

  return HAL_GPIO_ReadPin(s_ssr_pins[output].port, s_ssr_pins[output].pin);
}

void SSR_Control_AllOff(void)
{
  uint32_t index;

  for (index = 0U; index < (uint32_t)SSR_OUTPUT_COUNT; index++)
  {
    HAL_GPIO_WritePin(s_ssr_pins[index].port,
                      s_ssr_pins[index].pin,
                      GPIO_PIN_RESET);
  }
}
