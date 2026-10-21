/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.c
  * @author  MCD Application Team
  * @brief   ThreadX applicative file
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

/* Includes ------------------------------------------------------------------*/
#include "app_threadx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include "pwm_ssr.h"
#include "sensor_task.h"
#include "vibnet_protocol.h"
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define LED_DIAG_THREAD_STACK_SIZE  (1024U)
#define LED_DIAG_THREAD_PRIO        (20U)
#define PWM_BREATH_THREAD_STACK_SIZE (1024U)
#define PWM_BREATH_THREAD_PRIO       (21U)
#define PWM_BREATH_STEP              (10U)
#define SSR_BLINK_THREAD_STACK_SIZE  (1024U)
#define SSR_BLINK_THREAD_PRIO        (22U)

#define SENSOR_THREAD_STACK_SIZE      (2048U)
#define SENSOR_THREAD_PRIO            (18U)
#define SENSOR_TX_THREAD_STACK_SIZE   (2048U)
#define SENSOR_TX_THREAD_PRIO         (19U)
#define SENSOR_STREAM_BLOCK_COUNT     (8U)
#define SENSOR_UART_DMA_TIMEOUT_TICKS \
  ((TX_TIMER_TICKS_PER_SECOND + 9U) / 10U)
#define SENSOR_DCACHE_LINE_SIZE        (32U)
#define SENSOR_TX_FRAME_CACHE_SIZE     \
  ((VIBNET_PROTOCOL_MAX_FRAME_SIZE + SENSOR_DCACHE_LINE_SIZE - 1U) & \
   ~(SENSOR_DCACHE_LINE_SIZE - 1U))

/*
 * Select the ADXL357 connection used on the board.
 * 1: SPI1 + ADC1_CS(PF4), 0: I2C2 + ASEL0 address(0x1D)
 */
#define ADXL357_SENSOR_USE_SPI         (1U)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

static TX_THREAD led_diag_thread;
static TX_THREAD pwm_breath_thread;
static TX_THREAD ssr_blink_thread;
static TX_THREAD sensor_thread;
static TX_THREAD sensor_tx_thread;
static TX_MUTEX sensor_mutex;
static TX_QUEUE sensor_free_block_queue;
static TX_QUEUE sensor_ready_block_queue;
static TX_SEMAPHORE sensor_uart_tx_done;
static uint8_t sensor_api_ready;
static uint8_t sensor_stream_ready;
extern SPI_HandleTypeDef hspi1;
extern I2C_HandleTypeDef hi2c2;
extern UART_HandleTypeDef huart4;

__attribute__((aligned(8)))
static UCHAR led_diag_thread_stack[LED_DIAG_THREAD_STACK_SIZE];

__attribute__((aligned(8)))
static UCHAR pwm_breath_thread_stack[PWM_BREATH_THREAD_STACK_SIZE];

__attribute__((aligned(8)))
static UCHAR ssr_blink_thread_stack[SSR_BLINK_THREAD_STACK_SIZE];

__attribute__((aligned(8)))
static UCHAR sensor_thread_stack[SENSOR_THREAD_STACK_SIZE];

__attribute__((aligned(8)))
static UCHAR sensor_tx_thread_stack[SENSOR_TX_THREAD_STACK_SIZE];

__attribute__((aligned(8)))
static ULONG sensor_free_queue_storage[SENSOR_STREAM_BLOCK_COUNT];

__attribute__((aligned(8)))
static ULONG sensor_ready_queue_storage[SENSOR_STREAM_BLOCK_COUNT];

__attribute__((aligned(8)))
static SensorTask_FifoBlock_t sensor_stream_blocks[SENSOR_STREAM_BLOCK_COUNT];

__attribute__((aligned(SENSOR_DCACHE_LINE_SIZE)))
static uint8_t sensor_tx_frame[SENSOR_TX_FRAME_CACHE_SIZE];

static SensorTask_Context_t sensor_context;
static volatile uint32_t sensor_queued_blocks;
static volatile uint32_t sensor_dropped_blocks;
static volatile uint32_t sensor_transmitted_blocks;
static volatile uint32_t sensor_uart_errors;
static volatile uint8_t sensor_uart_tx_failed;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static VOID LED_Diag_Thread_Entry(ULONG thread_input);
static VOID PWM_Breath_Thread_Entry(ULONG thread_input);
static VOID SSR_Blink_Thread_Entry(ULONG thread_input);
static VOID Sensor_Thread_Entry(ULONG thread_input);
static VOID Sensor_Tx_Thread_Entry(ULONG thread_input);
static UINT Sensor_Stream_Init(void);
static uint16_t Vibnet_BuildSensorFrame(
        uint8_t *frame,
        uint16_t capacity,
        const SensorTask_FifoBlock_t *block,
        uint32_t sequence);
/* USER CODE END PFP */

/**
  * @brief  Application ThreadX Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT App_ThreadX_Init(VOID *memory_ptr)
{
  UINT ret = TX_SUCCESS;
  /* USER CODE BEGIN App_ThreadX_MEM_POOL */
  TX_PARAMETER_NOT_USED(memory_ptr);

  if (tx_mutex_create(&sensor_mutex,
                      "ADXL357 sensor mutex",
                      TX_INHERIT) != TX_SUCCESS)
  {
    return TX_MUTEX_ERROR;
  }

  sensor_api_ready = 1U;

  ret = Sensor_Stream_Init();
  if (ret != TX_SUCCESS)
  {
    return ret;
  }

  /* USER CODE END App_ThreadX_MEM_POOL */
  /* USER CODE BEGIN App_ThreadX_Init */

  /*
   * PH7 diagnostic LED thread runs concurrently with USBX.
   * Slow continuous blink means Application + ThreadX scheduler are alive.
   */
  if (tx_thread_create(&led_diag_thread,
                       "PH7 diagnostic LED",
                       LED_Diag_Thread_Entry,
                       0U,
                       led_diag_thread_stack,
                       sizeof(led_diag_thread_stack),
                       LED_DIAG_THREAD_PRIO,
                       LED_DIAG_THREAD_PRIO,
                       TX_NO_TIME_SLICE,
                       TX_AUTO_START) != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }

  if (tx_thread_create(&pwm_breath_thread,
                       "PWM breathing LEDs",
                       PWM_Breath_Thread_Entry,
                       0U,
                       pwm_breath_thread_stack,
                       sizeof(pwm_breath_thread_stack),
                       PWM_BREATH_THREAD_PRIO,
                       PWM_BREATH_THREAD_PRIO,
                       TX_NO_TIME_SLICE,
                       TX_AUTO_START) != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }

  if (tx_thread_create(&ssr_blink_thread,
                       "SSR 2-second blink",
                       SSR_Blink_Thread_Entry,
                       0U,
                       ssr_blink_thread_stack,
                       sizeof(ssr_blink_thread_stack),
                       SSR_BLINK_THREAD_PRIO,
                       SSR_BLINK_THREAD_PRIO,
                       TX_NO_TIME_SLICE,
                       TX_AUTO_START) != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }

  if (tx_thread_create(&sensor_thread,
                       "ADXL357 sensor",
                       Sensor_Thread_Entry,
                       0U,
                       sensor_thread_stack,
                       sizeof(sensor_thread_stack),
                       SENSOR_THREAD_PRIO,
                       SENSOR_THREAD_PRIO,
                       TX_NO_TIME_SLICE,
                       TX_AUTO_START) != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }

  if (tx_thread_create(&sensor_tx_thread,
                       "Sensor UART TX",
                       Sensor_Tx_Thread_Entry,
                       0U,
                       sensor_tx_thread_stack,
                       sizeof(sensor_tx_thread_stack),
                       SENSOR_TX_THREAD_PRIO,
                       SENSOR_TX_THREAD_PRIO,
                       TX_NO_TIME_SLICE,
                       TX_AUTO_START) != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }

  /* USER CODE END App_ThreadX_Init */

  return ret;
}

  /**
  * @brief  Function that implements the kernel's initialization.
  * @param  None
  * @retval None
  */
void MX_ThreadX_Init(void)
{
  /* USER CODE BEGIN Before_Kernel_Start */

  /*
   * tx_kernel_enter() transfers control to ThreadX.
   * The project already provides tx_application_define() in app_azure_rtos.c.
   */

  /* USER CODE END Before_Kernel_Start */

  tx_kernel_enter();

  /* USER CODE BEGIN Kernel_Start_Error */

  /* tx_kernel_enter() should never return. */
  Error_Handler();

  /* USER CODE END Kernel_Start_Error */
}

/* USER CODE BEGIN 1 */

_Static_assert(sizeof(SensorTask_FifoBlock_t *) == sizeof(ULONG),
               "ThreadX pointer queue requires one ULONG per pointer");

static void Vibnet_PutU16Le(uint8_t *destination, uint16_t value)
{
  destination[0] = (uint8_t)(value & 0xFFU);
  destination[1] = (uint8_t)((value >> 8) & 0xFFU);
}

static void Vibnet_PutU32Le(uint8_t *destination, uint32_t value)
{
  destination[0] = (uint8_t)(value & 0xFFU);
  destination[1] = (uint8_t)((value >> 8) & 0xFFU);
  destination[2] = (uint8_t)((value >> 16) & 0xFFU);
  destination[3] = (uint8_t)((value >> 24) & 0xFFU);
}

static uint32_t Vibnet_Crc32(const uint8_t *data, uint16_t length)
{
  uint32_t crc = 0xFFFFFFFFU;

  for (uint16_t index = 0U; index < length; index++)
  {
    crc ^= data[index];
    for (uint8_t bit = 0U; bit < 8U; bit++)
    {
      uint32_t mask = (uint32_t)(-(int32_t)(crc & 1U));
      crc = (crc >> 1) ^ (0xEDB88320U & mask);
    }
  }

  return ~crc;
}

static UINT Sensor_Stream_Init(void)
{
  UINT status;

  status = tx_semaphore_create(&sensor_uart_tx_done,
                               "UART4 DMA TX complete",
                               0U);
  if (status != TX_SUCCESS)
  {
    return status;
  }

  status = tx_queue_create(&sensor_free_block_queue,
                           "Sensor free blocks",
                           TX_1_ULONG,
                           sensor_free_queue_storage,
                           sizeof(sensor_free_queue_storage));
  if (status != TX_SUCCESS)
  {
    return status;
  }

  status = tx_queue_create(&sensor_ready_block_queue,
                           "Sensor ready blocks",
                           TX_1_ULONG,
                           sensor_ready_queue_storage,
                           sizeof(sensor_ready_queue_storage));
  if (status != TX_SUCCESS)
  {
    return status;
  }

  for (uint32_t index = 0U; index < SENSOR_STREAM_BLOCK_COUNT; index++)
  {
    SensorTask_FifoBlock_t *block = &sensor_stream_blocks[index];

    status = tx_queue_send(&sensor_free_block_queue, &block, TX_NO_WAIT);
    if (status != TX_SUCCESS)
    {
      return status;
    }
  }

  sensor_stream_ready = 1U;
  return TX_SUCCESS;
}

void SensorTask_OnFifo(const SensorTask_FifoBlock_t *source)
{
  SensorTask_FifoBlock_t *destination;
  UINT status;

  if ((sensor_stream_ready == 0U) || (source == NULL) ||
      (source->data_length == 0U) ||
      (source->data_length > sizeof(source->fifo_data)))
  {
    return;
  }

  status = tx_queue_receive(&sensor_free_block_queue,
                            &destination,
                            TX_NO_WAIT);
  if (status != TX_SUCCESS)
  {
    sensor_dropped_blocks++;
    return;
  }

  destination->timestamp_ms = source->timestamp_ms;
  destination->data_length = source->data_length;
  destination->sample_count = source->sample_count;
  memcpy(destination->fifo_data,
         source->fifo_data,
         source->data_length);

  status = tx_queue_send(&sensor_ready_block_queue,
                         &destination,
                         TX_NO_WAIT);
  if (status != TX_SUCCESS)
  {
    sensor_dropped_blocks++;
    (void)tx_queue_send(&sensor_free_block_queue,
                        &destination,
                        TX_NO_WAIT);
    return;
  }

  sensor_queued_blocks++;
}

static uint16_t Vibnet_BuildSensorFrame(
        uint8_t *frame,
        uint16_t capacity,
        const SensorTask_FifoBlock_t *block,
        uint32_t sequence)
{
  uint16_t frame_without_crc;
  uint16_t frame_length;
  uint32_t crc;

  if ((frame == NULL) || (block == NULL) ||
      (block->data_length > VIBNET_PROTOCOL_MAX_PAYLOAD_SIZE))
  {
    return 0U;
  }

  frame_without_crc =
      (uint16_t)(VIBNET_PROTOCOL_HEADER_SIZE + block->data_length);
  frame_length =
      (uint16_t)(frame_without_crc + VIBNET_PROTOCOL_CRC_SIZE);
  if (frame_length > capacity)
  {
    return 0U;
  }

  frame[0] = VIBNET_PROTOCOL_MAGIC_0;
  frame[1] = VIBNET_PROTOCOL_MAGIC_1;
  frame[2] = VIBNET_PROTOCOL_VERSION;
  frame[3] = VIBNET_MESSAGE_SENSOR_FIFO;
  Vibnet_PutU16Le(&frame[4], 0U);
  Vibnet_PutU16Le(&frame[6], VIBNET_PROTOCOL_HEADER_SIZE);
  Vibnet_PutU32Le(&frame[8], sequence);
  Vibnet_PutU32Le(&frame[12], block->timestamp_ms);
  Vibnet_PutU16Le(&frame[16], block->data_length);
  Vibnet_PutU16Le(&frame[18], block->sample_count);
  memcpy(&frame[VIBNET_PROTOCOL_HEADER_SIZE],
         block->fifo_data,
         block->data_length);

  crc = Vibnet_Crc32(frame, frame_without_crc);
  Vibnet_PutU32Le(&frame[frame_without_crc], crc);
  return frame_length;
}

static VOID Sensor_Tx_Thread_Entry(ULONG thread_input)
{
  SensorTask_FifoBlock_t *block;
  uint32_t sequence = 0U;
  uint16_t frame_length;
  UINT wait_status;

  TX_PARAMETER_NOT_USED(thread_input);

  while (1)
  {
    if (tx_queue_receive(&sensor_ready_block_queue,
                         &block,
                         TX_WAIT_FOREVER) != TX_SUCCESS)
    {
      continue;
    }

    frame_length = Vibnet_BuildSensorFrame(
            sensor_tx_frame,
            sizeof(sensor_tx_frame),
            block,
            sequence++);

    while (tx_semaphore_get(&sensor_uart_tx_done, TX_NO_WAIT) == TX_SUCCESS)
    {
      /* Discard a late completion left by an aborted transfer. */
    }

    sensor_uart_tx_failed = 0U;
    wait_status = TX_NO_INSTANCE;

    if (frame_length != 0U)
    {
      /* UART4 GPDMA reads this cacheable SRAM buffer directly. */
      SCB_CleanDCache_by_Addr(sensor_tx_frame,
                             (int32_t)SENSOR_TX_FRAME_CACHE_SIZE);
      __DSB();

      if (HAL_UART_Transmit_DMA(&huart4,
                                sensor_tx_frame,
                                frame_length) == HAL_OK)
      {
        wait_status = tx_semaphore_get(&sensor_uart_tx_done,
                                       SENSOR_UART_DMA_TIMEOUT_TICKS);
      }
    }

    if ((frame_length == 0U) ||
        (wait_status != TX_SUCCESS) ||
        (sensor_uart_tx_failed != 0U))
    {
      if (huart4.gState != HAL_UART_STATE_READY)
      {
        (void)HAL_UART_AbortTransmit(&huart4);
      }
      sensor_uart_errors++;
    }
    else
    {
      sensor_transmitted_blocks++;
    }

    if (tx_queue_send(&sensor_free_block_queue,
                      &block,
                      TX_WAIT_FOREVER) != TX_SUCCESS)
    {
      sensor_dropped_blocks++;
    }
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if ((huart != NULL) && (huart->Instance == UART4) &&
      (sensor_stream_ready != 0U))
  {
    (void)tx_semaphore_put(&sensor_uart_tx_done);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if ((huart != NULL) && (huart->Instance == UART4) &&
      (sensor_stream_ready != 0U))
  {
    sensor_uart_tx_failed = 1U;
    (void)tx_semaphore_put(&sensor_uart_tx_done);
  }
}

static VOID Sensor_Thread_Entry(ULONG thread_input)
{
  HAL_StatusTypeDef status;
  SensorTask_FifoBlock_t fifo_block;
  ULONG sleep_ticks;

  TX_PARAMETER_NOT_USED(thread_input);

  while (1)
  {
    status = HAL_OK;
    sleep_ticks = 1U;

    if (tx_mutex_get(&sensor_mutex, TX_WAIT_FOREVER) != TX_SUCCESS)
    {
      tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND);
      continue;
    }

    if ((sensor_context.state == SENSOR_TASK_STATE_UNINITIALIZED) ||
        (sensor_context.state == SENSOR_TASK_STATE_ERROR))
    {
#if ADXL357_SENSOR_USE_SPI
      status = SensorTask_InitSPI(&sensor_context,
                                  &hspi1,
                                  ADC1_CS_GPIO_Port,
                                  ADC1_CS_Pin);
#else
      status = SensorTask_InitI2C(&sensor_context,
                                  &hi2c2,
                                  ADXL357_I2C_ADDRESS_ASEL0);
#endif
      if (status != HAL_OK)
      {
        sleep_ticks = TX_TIMER_TICKS_PER_SECOND;
      }
    }
    else if (sensor_context.state == SENSOR_TASK_STATE_RUNNING)
    {
      status = SensorTask_ProcessFifo(&sensor_context, &fifo_block);
      if (status != HAL_OK)
      {
        sleep_ticks = TX_TIMER_TICKS_PER_SECOND;
      }
    }

    (void)tx_mutex_put(&sensor_mutex);

    if (sleep_ticks == 1U)
    {
      /* FIFO polling skeleton. Replace this sleep with an ADXL INT semaphore. */
      tx_thread_sleep(1U);
    }
    else
    {
      tx_thread_sleep(sleep_ticks);
    }
  }
}

HAL_StatusTypeDef VibnetSensor_Start(void)
{
  HAL_StatusTypeDef status;

  if ((sensor_api_ready == 0U) ||
      (tx_mutex_get(&sensor_mutex, TX_WAIT_FOREVER) != TX_SUCCESS))
  {
    return HAL_ERROR;
  }

  status = SensorTask_Start(&sensor_context);
  (void)tx_mutex_put(&sensor_mutex);
  return status;
}

HAL_StatusTypeDef VibnetSensor_Stop(void)
{
  HAL_StatusTypeDef status;

  if ((sensor_api_ready == 0U) ||
      (tx_mutex_get(&sensor_mutex, TX_WAIT_FOREVER) != TX_SUCCESS))
  {
    return HAL_ERROR;
  }

  if (sensor_context.state == SENSOR_TASK_STATE_UNINITIALIZED)
  {
    status = HAL_ERROR;
  }
  else
  {
    SensorTask_Stop(&sensor_context);
    status = sensor_context.last_error;
  }

  (void)tx_mutex_put(&sensor_mutex);
  return status;
}

HAL_StatusTypeDef VibnetSensor_ApplyConfig(
        const SensorTask_Config_t *config)
{
  HAL_StatusTypeDef status;

  if ((sensor_api_ready == 0U) || (config == NULL) ||
      (tx_mutex_get(&sensor_mutex, TX_WAIT_FOREVER) != TX_SUCCESS))
  {
    return HAL_ERROR;
  }

  status = SensorTask_ApplyConfig(&sensor_context, config);
  (void)tx_mutex_put(&sensor_mutex);
  return status;
}

HAL_StatusTypeDef VibnetSensor_SetRangeG(uint16_t range_g)
{
  HAL_StatusTypeDef status;
  ADXL357_Range_t range;

  switch (range_g)
  {
    case 10U:
      range = ADXL357_RANGE_10G;
      break;

    case 20U:
      range = ADXL357_RANGE_20G;
      break;

    case 40U:
      range = ADXL357_RANGE_40G;
      break;

    default:
      return HAL_ERROR;
  }

  if ((sensor_api_ready == 0U) ||
      (tx_mutex_get(&sensor_mutex, TX_WAIT_FOREVER) != TX_SUCCESS))
  {
    return HAL_ERROR;
  }

  status = SensorTask_SetRange(&sensor_context, range);
  (void)tx_mutex_put(&sensor_mutex);
  return status;
}

HAL_StatusTypeDef VibnetSensor_SetFilter(
        ADXL357_HPF_t hpf,
        ADXL357_ODR_t odr)
{
  HAL_StatusTypeDef status;

  if ((sensor_api_ready == 0U) ||
      (tx_mutex_get(&sensor_mutex, TX_WAIT_FOREVER) != TX_SUCCESS))
  {
    return HAL_ERROR;
  }

  status = SensorTask_SetFilter(&sensor_context, hpf, odr);
  (void)tx_mutex_put(&sensor_mutex);
  return status;
}

HAL_StatusTypeDef VibnetSensor_SetSensitivity(
        float x_sensitivity,
        float y_sensitivity,
        float z_sensitivity)
{
  HAL_StatusTypeDef status;

  if ((sensor_api_ready == 0U) ||
      (tx_mutex_get(&sensor_mutex, TX_WAIT_FOREVER) != TX_SUCCESS))
  {
    return HAL_ERROR;
  }

  status = SensorTask_SetSensitivity(
          &sensor_context,
          x_sensitivity,
          y_sensitivity,
          z_sensitivity);
  (void)tx_mutex_put(&sensor_mutex);
  return status;
}

HAL_StatusTypeDef VibnetSensor_GetConfig(
        SensorTask_Config_t *config)
{
  HAL_StatusTypeDef status;

  if ((sensor_api_ready == 0U) || (config == NULL) ||
      (tx_mutex_get(&sensor_mutex, TX_WAIT_FOREVER) != TX_SUCCESS))
  {
    return HAL_ERROR;
  }

  status = SensorTask_GetConfig(&sensor_context, config);
  (void)tx_mutex_put(&sensor_mutex);
  return status;
}

HAL_StatusTypeDef VibnetSensor_GetLatest(
        ADXL357_Axes_t *axes)
{
  HAL_StatusTypeDef status;

  if ((sensor_api_ready == 0U) || (axes == NULL) ||
      (tx_mutex_get(&sensor_mutex, TX_WAIT_FOREVER) != TX_SUCCESS))
  {
    return HAL_ERROR;
  }

  status = SensorTask_GetLatest(&sensor_context, axes);
  (void)tx_mutex_put(&sensor_mutex);
  return status;
}

void VibnetSensor_GetStreamStats(VibnetStreamStats_t *stats)
{
  if (stats == NULL)
  {
    return;
  }

  stats->queued_blocks = sensor_queued_blocks;
  stats->dropped_blocks = sensor_dropped_blocks;
  stats->transmitted_blocks = sensor_transmitted_blocks;
  stats->uart_errors = sensor_uart_errors;
}

static VOID LED_Diag_Thread_Entry(ULONG thread_input)
{
  GPIO_InitTypeDef gpio = {0};

  TX_PARAMETER_NOT_USED(thread_input);

  __HAL_RCC_GPIOH_CLK_ENABLE();

  gpio.Pin = GPIO_PIN_7;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOH, &gpio);

  while (1)
  {
    if (g_usb2_enum_done_seen != 0U)
    {
      /* Solid ON: the host completed USB speed enumeration. */
      HAL_GPIO_WritePin(GPIOH, GPIO_PIN_7, GPIO_PIN_RESET);
      tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND);
    }
    else if (g_usb2_reset_seen != 0U)
    {
      /* Two short flashes: the PC detected the device and issued USB reset. */
      for (uint32_t pulse = 0U; pulse < 2U; pulse++)
      {
        HAL_GPIO_WritePin(GPIOH, GPIO_PIN_7, GPIO_PIN_RESET);
        tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 10U);
        HAL_GPIO_WritePin(GPIOH, GPIO_PIN_7, GPIO_PIN_SET);
        tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 10U);
      }
      tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND);
    }
    else if (g_usb2_irq_seen != 0U)
    {
      /* Fast blink: USB2 generated an interrupt but no bus reset was seen. */
      HAL_GPIO_TogglePin(GPIOH, GPIO_PIN_7);
      tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 10U);
    }
    else
    {
      /* Slow heartbeat: Application/ThreadX run, but the PC sees no USB2 bus. */
      HAL_GPIO_TogglePin(GPIOH, GPIO_PIN_7);
      tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 2U);
    }
  }
}

static VOID PWM_Breath_Thread_Entry(ULONG thread_input)
{
  uint16_t level = PWM_CONTROL_MIN_VALUE;
  uint8_t rising = 1U;
  ULONG delay_ticks = TX_TIMER_TICKS_PER_SECOND / 100U;

  TX_PARAMETER_NOT_USED(thread_input);

  if (delay_ticks == 0U)
  {
    delay_ticks = 1U;
  }

  while (1)
  {
    (void)PWM_Control_Set(PWM_OUTPUT_TIM4, level);
    (void)PWM_Control_Set(PWM_OUTPUT_TIM8, level);
    (void)PWM_Control_Set(PWM_OUTPUT_TIM14, level);
    (void)PWM_Control_Set(PWM_OUTPUT_TIM15, level);

    tx_thread_sleep(delay_ticks);

    if (rising != 0U)
    {
      if (level >= (PWM_CONTROL_MAX_VALUE - PWM_BREATH_STEP))
      {
        level = PWM_CONTROL_MAX_VALUE;
        rising = 0U;
      }
      else
      {
        level = (uint16_t)(level + PWM_BREATH_STEP);
      }
    }
    else
    {
      if (level <= PWM_BREATH_STEP)
      {
        level = PWM_CONTROL_MIN_VALUE;
        rising = 1U;
      }
      else
      {
        level = (uint16_t)(level - PWM_BREATH_STEP);
      }
    }
  }
}

static VOID SSR_Blink_Thread_Entry(ULONG thread_input)
{
  uint8_t on = 1U;

  TX_PARAMETER_NOT_USED(thread_input);

  while (1)
  {
    (void)SSR_Control_Set(SSR_OUTPUT_1, on);
    (void)SSR_Control_Set(SSR_OUTPUT_2, on);
    (void)SSR_Control_Set(SSR_OUTPUT_3, on);
    (void)SSR_Control_Set(SSR_OUTPUT_4, on);
    (void)SSR_Control_Set(SSR_OUTPUT_5, on);

    tx_thread_sleep(2U * TX_TIMER_TICKS_PER_SECOND);
    on = (on == 0U) ? 1U : 0U;
  }
}

/* USER CODE END 1 */
