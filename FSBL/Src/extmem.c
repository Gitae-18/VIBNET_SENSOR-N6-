#include "extmem.h"
#include "main.h"

void MX_EXTMEM_Init(void)
{
  if (EXTMEM_Init(EXTMEMORY_1,
                  HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_XSPI2)) != EXTMEM_OK)
  {
    Error_Handler();
  }
}
