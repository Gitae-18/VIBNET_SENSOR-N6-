#include "psram_test.h"
#include "stm32n6xx_hal_xspi.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define PSRAM_READ_CMD                    0x00U
#define PSRAM_WRITE_CMD                   0x80U
#define PSRAM_READ_REG_CMD                0x40U
#define PSRAM_WRITE_REG_CMD               0xC0U
#define PSRAM_MR0                         0x00000000UL
#define PSRAM_MR4                         0x00000004UL
#define PSRAM_MR8                         0x00000008UL
#define PSRAM_DUMMY_CYCLES_READ           6U
#define PSRAM_DUMMY_CYCLES_WRITE          6U
#define PSRAM_WORD_COUNT                  (PSRAM_TEST_SIZE_BYTES / sizeof(uint32_t))

typedef enum
{
  PSRAM_PATTERN_CONSTANT = 0,
  PSRAM_PATTERN_ADDRESS,
  PSRAM_PATTERN_INVERSE_ADDRESS
} PSRAM_PatternType_t;

static XSPI_HandleTypeDef s_hxspi1;

static void PSRAM_Log(UART_HandleTypeDef *huart, const char *format, ...);
static HAL_StatusTypeDef PSRAM_XSPI_Init(void);
static HAL_StatusTypeDef PSRAM_WriteRegister(uint32_t address, uint8_t value[2]);
static HAL_StatusTypeDef PSRAM_ReadRegister(uint32_t address,
                                            uint8_t value[2],
                                            uint32_t latency);
static HAL_StatusTypeDef PSRAM_ConfigureDevice(void);
static HAL_StatusTypeDef PSRAM_EnableMemoryMapped(void);
static HAL_StatusTypeDef PSRAM_RunPattern(UART_HandleTypeDef *huart,
                                          const char *name,
                                          PSRAM_PatternType_t type,
                                          uint32_t constant);
static uint32_t PSRAM_ExpectedValue(PSRAM_PatternType_t type,
                                    uint32_t address,
                                    uint32_t constant);
static void PSRAM_CacheCommitAndInvalidate(void);

void HAL_XSPI_MspInit(XSPI_HandleTypeDef *hxspi)
{
  GPIO_InitTypeDef gpio = {0};
  RCC_PeriphCLKInitTypeDef periph_clock = {0};

  if (hxspi->Instance != XSPI1)
  {
    return;
  }

  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWREx_EnableVddIO2();
  HAL_PWREx_ConfigVddIORange(PWR_VDDIO2, PWR_VDDIO_RANGE_1V8);

  periph_clock.PeriphClockSelection = RCC_PERIPHCLK_XSPI1;
  periph_clock.Xspi1ClockSelection = RCC_XSPI1CLKSOURCE_HCLK;
  if (HAL_RCCEx_PeriphCLKConfig(&periph_clock) != HAL_OK)
  {
    return;
  }

  __HAL_RCC_XSPIM_CLK_ENABLE();
  __HAL_RCC_XSPI1_CLK_ENABLE();
  __HAL_RCC_GPIOO_CLK_ENABLE();
  __HAL_RCC_GPIOP_CLK_ENABLE();

  HAL_GPIO_ConfigPinAttributes(GPIOO,
                               GPIO_PIN_0 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4,
                               GPIO_PIN_SEC | GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP, GPIO_PIN_ALL,
                               GPIO_PIN_SEC | GPIO_PIN_NPRIV);

  gpio.Pin = GPIO_PIN_ALL;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.Alternate = GPIO_AF9_XSPIM_P1;
  HAL_GPIO_Init(GPIOP, &gpio);

  gpio.Pin = GPIO_PIN_0 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4;
  HAL_GPIO_Init(GPIOO, &gpio);
}

HAL_StatusTypeDef PSRAM_FullTest(UART_HandleTypeDef *huart)
{
  HAL_StatusTypeDef overall = HAL_OK;

  if (huart == NULL)
  {
    return HAL_ERROR;
  }

  PSRAM_Log(huart, "\r\n=== APS256 PSRAM FULL TEST (32 MiB) ===\r\n");
  PSRAM_Log(huart, "[PSRAM] Destructive test: 0x%08lX - 0x%08lX\r\n",
            (unsigned long)PSRAM_TEST_BASE_ADDRESS,
            (unsigned long)(PSRAM_TEST_BASE_ADDRESS + PSRAM_TEST_SIZE_BYTES - 1UL));

  if (PSRAM_XSPI_Init() != HAL_OK)
  {
    PSRAM_Log(huart, "[PSRAM] XSPI1 INIT FAIL\r\n");
    return HAL_ERROR;
  }
  if (PSRAM_ConfigureDevice() != HAL_OK)
  {
    PSRAM_Log(huart, "[PSRAM] APS256 REGISTER CONFIG FAIL\r\n");
    return HAL_ERROR;
  }
  if (PSRAM_EnableMemoryMapped() != HAL_OK)
  {
    PSRAM_Log(huart, "[PSRAM] MEMORY-MAPPED MODE FAIL\r\n");
    return HAL_ERROR;
  }
  PSRAM_Log(huart, "[PSRAM] INIT OK / MEMORY-MAPPED OK\r\n");

  if (PSRAM_RunPattern(huart, "0x00000000",
                       PSRAM_PATTERN_CONSTANT, 0x00000000UL) != HAL_OK)
  {
    overall = HAL_ERROR;
  }
  if ((overall == HAL_OK) &&
      (PSRAM_RunPattern(huart, "0xFFFFFFFF",
                        PSRAM_PATTERN_CONSTANT, 0xFFFFFFFFUL) != HAL_OK))
  {
    overall = HAL_ERROR;
  }
  if ((overall == HAL_OK) &&
      (PSRAM_RunPattern(huart, "0xAAAAAAAA",
                        PSRAM_PATTERN_CONSTANT, 0xAAAAAAAAUL) != HAL_OK))
  {
    overall = HAL_ERROR;
  }
  if ((overall == HAL_OK) &&
      (PSRAM_RunPattern(huart, "0x55555555",
                        PSRAM_PATTERN_CONSTANT, 0x55555555UL) != HAL_OK))
  {
    overall = HAL_ERROR;
  }
  if ((overall == HAL_OK) &&
      (PSRAM_RunPattern(huart, "ASCII 'A' (0x41414141)",
                        PSRAM_PATTERN_CONSTANT, 0x41414141UL) != HAL_OK))
  {
    overall = HAL_ERROR;
  }
  if ((overall == HAL_OK) &&
      (PSRAM_RunPattern(huart, "ADDRESS",
                        PSRAM_PATTERN_ADDRESS, 0UL) != HAL_OK))
  {
    overall = HAL_ERROR;
  }
  if ((overall == HAL_OK) &&
      (PSRAM_RunPattern(huart, "INVERSE ADDRESS",
                        PSRAM_PATTERN_INVERSE_ADDRESS, 0UL) != HAL_OK))
  {
    overall = HAL_ERROR;
  }

  /* Always make a best effort to leave the complete PSRAM cleared. */
  if (PSRAM_RunPattern(huart, "CLEAR (0x00000000)",
                       PSRAM_PATTERN_CONSTANT, 0x00000000UL) != HAL_OK)
  {
    overall = HAL_ERROR;
  }

  if (overall == HAL_OK)
  {
    PSRAM_Log(huart, "[PSRAM] FULL TEST PASS\r\n");
  }
  else
  {
    PSRAM_Log(huart, "[PSRAM] FULL TEST FAIL\r\n");
  }

  return overall;
}

static void PSRAM_Log(UART_HandleTypeDef *huart, const char *format, ...)
{
  char buffer[180];
  va_list args;
  int length;

  va_start(args, format);
  length = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  if (length <= 0)
  {
    return;
  }
  if ((size_t)length >= sizeof(buffer))
  {
    length = (int)(sizeof(buffer) - 1U);
  }

  (void)HAL_UART_Transmit(huart, (uint8_t *)buffer, (uint16_t)length, 2000U);
}

static HAL_StatusTypeDef PSRAM_XSPI_Init(void)
{
  XSPIM_CfgTypeDef manager = {0};

  memset(&s_hxspi1, 0, sizeof(s_hxspi1));
  s_hxspi1.Instance = XSPI1;
  s_hxspi1.Init.FifoThresholdByte = 4;
  s_hxspi1.Init.MemoryMode = HAL_XSPI_SINGLE_MEM;
  s_hxspi1.Init.MemoryType = HAL_XSPI_MEMTYPE_APMEM_16BITS;
  s_hxspi1.Init.MemorySize = HAL_XSPI_SIZE_256MB;
  s_hxspi1.Init.ChipSelectHighTimeCycle = 5;
  s_hxspi1.Init.FreeRunningClock = HAL_XSPI_FREERUNCLK_DISABLE;
  s_hxspi1.Init.ClockMode = HAL_XSPI_CLOCK_MODE_0;
  s_hxspi1.Init.WrapSize = HAL_XSPI_WRAP_NOT_SUPPORTED;
  s_hxspi1.Init.ClockPrescaler = 1;
  s_hxspi1.Init.SampleShifting = HAL_XSPI_SAMPLE_SHIFT_NONE;
  s_hxspi1.Init.DelayHoldQuarterCycle = HAL_XSPI_DHQC_DISABLE;
  s_hxspi1.Init.ChipSelectBoundary = HAL_XSPI_BONDARYOF_4KB;
  s_hxspi1.Init.MaxTran = 0;
  s_hxspi1.Init.Refresh = 0;
  s_hxspi1.Init.MemorySelect = HAL_XSPI_CSSEL_NCS1;

  if (HAL_XSPI_Init(&s_hxspi1) != HAL_OK)
  {
    return HAL_ERROR;
  }

  manager.nCSOverride = HAL_XSPI_CSSEL_OVR_NCS1;
  manager.IOPort = HAL_XSPIM_IOPORT_1;
  manager.Req2AckTime = 1;
  return HAL_XSPIM_Config(&s_hxspi1, &manager, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
}

static HAL_StatusTypeDef PSRAM_WriteRegister(uint32_t address, uint8_t value[2])
{
  XSPI_RegularCmdTypeDef command = {0};

  command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  command.InstructionMode = HAL_XSPI_INSTRUCTION_8_LINES;
  command.InstructionWidth = HAL_XSPI_INSTRUCTION_8_BITS;
  command.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  command.Instruction = PSRAM_WRITE_REG_CMD;
  command.AddressMode = HAL_XSPI_ADDRESS_8_LINES;
  command.AddressWidth = HAL_XSPI_ADDRESS_32_BITS;
  command.AddressDTRMode = HAL_XSPI_ADDRESS_DTR_ENABLE;
  command.Address = address;
  command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  command.DataMode = HAL_XSPI_DATA_8_LINES;
  command.DataDTRMode = HAL_XSPI_DATA_DTR_ENABLE;
  command.DataLength = 2;
  command.DummyCycles = 0;
  command.DQSMode = HAL_XSPI_DQS_DISABLE;

  if (HAL_XSPI_Command(&s_hxspi1, &command,
                       HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return HAL_ERROR;
  }
  return HAL_XSPI_Transmit(&s_hxspi1, value, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
}

static HAL_StatusTypeDef PSRAM_ReadRegister(uint32_t address,
                                            uint8_t value[2],
                                            uint32_t latency)
{
  XSPI_RegularCmdTypeDef command = {0};

  command.OperationType = HAL_XSPI_OPTYPE_COMMON_CFG;
  command.InstructionMode = HAL_XSPI_INSTRUCTION_8_LINES;
  command.InstructionWidth = HAL_XSPI_INSTRUCTION_8_BITS;
  command.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  command.Instruction = PSRAM_READ_REG_CMD;
  command.AddressMode = HAL_XSPI_ADDRESS_8_LINES;
  command.AddressWidth = HAL_XSPI_ADDRESS_32_BITS;
  command.AddressDTRMode = HAL_XSPI_ADDRESS_DTR_ENABLE;
  command.Address = address;
  command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  command.DataMode = HAL_XSPI_DATA_8_LINES;
  command.DataDTRMode = HAL_XSPI_DATA_DTR_ENABLE;
  command.DataLength = 2;
  command.DummyCycles = latency - 1U;
  command.DQSMode = HAL_XSPI_DQS_ENABLE;

  if (HAL_XSPI_Command(&s_hxspi1, &command,
                       HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return HAL_ERROR;
  }
  return HAL_XSPI_Receive(&s_hxspi1, value, HAL_XSPI_TIMEOUT_DEFAULT_VALUE);
}

static HAL_StatusTypeDef PSRAM_ConfigureDevice(void)
{
  uint8_t write_mr0[2] = {0x30U, 0x8DU};
  uint8_t write_mr4[2] = {0x20U, 0xF0U};
  uint8_t write_mr8[2] = {0x4BU, 0x08U};
  uint8_t read_value[2] = {0U, 0U};

  if ((PSRAM_WriteRegister(PSRAM_MR0, write_mr0) != HAL_OK) ||
      (PSRAM_ReadRegister(PSRAM_MR0, read_value, 6U) != HAL_OK) ||
      (read_value[0] != write_mr0[0]))
  {
    return HAL_ERROR;
  }

  if ((PSRAM_WriteRegister(PSRAM_MR4, write_mr4) != HAL_OK) ||
      (PSRAM_ReadRegister(PSRAM_MR4, read_value, 6U) != HAL_OK) ||
      (read_value[0] != write_mr4[0]))
  {
    return HAL_ERROR;
  }

  if ((PSRAM_WriteRegister(PSRAM_MR8, write_mr8) != HAL_OK) ||
      (PSRAM_ReadRegister(PSRAM_MR8, read_value, 6U) != HAL_OK) ||
      (read_value[0] != write_mr8[0]))
  {
    return HAL_ERROR;
  }

  /* HCLK is 100 MHz in this project: divider (1 + 1) keeps XSPI1 at 50 MHz. */
  return HAL_XSPI_SetClockPrescaler(&s_hxspi1, 1U);
}

static HAL_StatusTypeDef PSRAM_EnableMemoryMapped(void)
{
  XSPI_RegularCmdTypeDef command = {0};
  XSPI_MemoryMappedTypeDef memory_mapped = {0};

  command.OperationType = HAL_XSPI_OPTYPE_WRITE_CFG;
  command.InstructionMode = HAL_XSPI_INSTRUCTION_8_LINES;
  command.InstructionWidth = HAL_XSPI_INSTRUCTION_8_BITS;
  command.InstructionDTRMode = HAL_XSPI_INSTRUCTION_DTR_DISABLE;
  command.Instruction = PSRAM_WRITE_CMD;
  command.AddressMode = HAL_XSPI_ADDRESS_8_LINES;
  command.AddressWidth = HAL_XSPI_ADDRESS_32_BITS;
  command.AddressDTRMode = HAL_XSPI_ADDRESS_DTR_ENABLE;
  command.Address = 0U;
  command.AlternateBytesMode = HAL_XSPI_ALT_BYTES_NONE;
  command.DataMode = HAL_XSPI_DATA_16_LINES;
  command.DataDTRMode = HAL_XSPI_DATA_DTR_ENABLE;
  command.DataLength = 0U;
  command.DummyCycles = PSRAM_DUMMY_CYCLES_WRITE;
  command.DQSMode = HAL_XSPI_DQS_ENABLE;

  if (HAL_XSPI_Command(&s_hxspi1, &command,
                       HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return HAL_ERROR;
  }

  command.OperationType = HAL_XSPI_OPTYPE_READ_CFG;
  command.Instruction = PSRAM_READ_CMD;
  command.DummyCycles = PSRAM_DUMMY_CYCLES_READ;

  if (HAL_XSPI_Command(&s_hxspi1, &command,
                       HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    return HAL_ERROR;
  }

  memory_mapped.TimeOutActivation = HAL_XSPI_TIMEOUT_COUNTER_ENABLE;
  memory_mapped.TimeoutPeriodClock = 0x34U;
  return HAL_XSPI_MemoryMapped(&s_hxspi1, &memory_mapped);
}

static HAL_StatusTypeDef PSRAM_RunPattern(UART_HandleTypeDef *huart,
                                          const char *name,
                                          PSRAM_PatternType_t type,
                                          uint32_t constant)
{
  volatile uint32_t *memory =
      (volatile uint32_t *)(uintptr_t)PSRAM_TEST_BASE_ADDRESS;
  uint32_t index;
  uint32_t address;
  uint32_t expected;
  uint32_t actual;

  PSRAM_Log(huart, "[PSRAM] %-26s WRITE...\r\n", name);

  for (index = 0U; index < PSRAM_WORD_COUNT; index++)
  {
    address = PSRAM_TEST_BASE_ADDRESS + (index * sizeof(uint32_t));
    memory[index] = PSRAM_ExpectedValue(type, address, constant);
  }

  PSRAM_CacheCommitAndInvalidate();
  PSRAM_Log(huart, "[PSRAM] %-26s VERIFY...\r\n", name);

  for (index = 0U; index < PSRAM_WORD_COUNT; index++)
  {
    address = PSRAM_TEST_BASE_ADDRESS + (index * sizeof(uint32_t));
    expected = PSRAM_ExpectedValue(type, address, constant);
    actual = memory[index];
    if (actual != expected)
    {
      PSRAM_Log(huart,
                "[PSRAM] FAIL ADDR=0x%08lX EXP=0x%08lX ACT=0x%08lX\r\n",
                (unsigned long)address,
                (unsigned long)expected,
                (unsigned long)actual);
      return HAL_ERROR;
    }
  }

  PSRAM_Log(huart, "[PSRAM] %-26s PASS\r\n", name);
  return HAL_OK;
}

static uint32_t PSRAM_ExpectedValue(PSRAM_PatternType_t type,
                                    uint32_t address,
                                    uint32_t constant)
{
  if (type == PSRAM_PATTERN_ADDRESS)
  {
    return address;
  }
  if (type == PSRAM_PATTERN_INVERSE_ADDRESS)
  {
    return ~address;
  }
  return constant;
}

static void PSRAM_CacheCommitAndInvalidate(void)
{
  SCB_CleanDCache_by_Addr((void *)(uintptr_t)PSRAM_TEST_BASE_ADDRESS,
                         (int32_t)PSRAM_TEST_SIZE_BYTES);
  __DSB();
  SCB_InvalidateDCache_by_Addr((void *)(uintptr_t)PSRAM_TEST_BASE_ADDRESS,
                              (int32_t)PSRAM_TEST_SIZE_BYTES);
  __DSB();
  __ISB();
}
