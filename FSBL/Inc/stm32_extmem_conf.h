#ifndef STM32_EXTMEM_CONF_H
#define STM32_EXTMEM_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#define EXTMEM_DRIVER_NOR_SFDP  1
#define EXTMEM_DRIVER_PSRAM     0
#define EXTMEM_SAL_XSPI         1

#include "stm32n6xx_hal.h"
#include "stm32_extmem.h"
#include "stm32_extmem_type.h"
#include "boot/stm32_boot_lrun.h"

extern XSPI_HandleTypeDef hxspi2;

enum
{
  EXTMEMORY_1 = 0
};

/* The signed Application image is stored at 0x70100000. Its 0x400-byte
 * header is copied to 0x34000000; the linked payload starts at 0x34000400. */
#define EXTMEM_LRUN_DESTINATION_INTERNAL
#define EXTMEM_LRUN_DESTINATION_ADDRESS  0x34000000U
#define EXTMEM_LRUN_SOURCE               EXTMEMORY_1
#define EXTMEM_LRUN_SOURCE_ADDRESS       0x00100000U
/* Copy enough room for the signed Application image.  The current image is
 * 0x11000 bytes including its 0x400-byte STM32 header; keep margin for growth. */
#define EXTMEM_LRUN_SOURCE_SIZE          0x00020000U
#define EXTMEM_HEADER_OFFSET             0x00000400U

extern EXTMEM_DefinitionTypeDef extmem_list_config[1];

#if defined(EXTMEM_C)
EXTMEM_DefinitionTypeDef extmem_list_config[1] =
{
  {
    .MemType = EXTMEM_NOR_SFDP,
    .Handle = (void *)&hxspi2,
    .ConfigType = EXTMEM_LINK_CONFIG_8LINES,
    .NorSfdpObject = { {0} }
  }
};
#endif

#define EXTMEM_DEBUG_LEVEL                  0
#define EXTMEM_DRIVER_NOR_SFDP_DEBUG_LEVEL  0
#define EXTMEM_DRIVER_PSRAM_DEBUG_LEVEL     0
#define EXTMEM_SAL_XSPI_DEBUG_LEVEL         0

#ifdef __cplusplus
}
#endif

#endif /* STM32_EXTMEM_CONF_H */
