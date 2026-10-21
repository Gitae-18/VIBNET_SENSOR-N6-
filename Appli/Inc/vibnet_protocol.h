#ifndef VIBNET_PROTOCOL_H
#define VIBNET_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * Wire format (little endian):
 *
 *   0  : magic[0]       ('V')
 *   1  : magic[1]       ('N')
 *   2  : version
 *   3  : message type
 *   4  : flags           (u16)
 *   6  : header length   (u16)
 *   8  : sequence        (u32)
 *   12 : timestamp ms    (u32)
 *   16 : payload length  (u16)
 *   18 : sample count    (u16)
 *   20 : payload
 *   .. : CRC32 over header and payload (u32)
 */
#define VIBNET_PROTOCOL_MAGIC_0             (0x56U)
#define VIBNET_PROTOCOL_MAGIC_1             (0x4EU)
#define VIBNET_PROTOCOL_VERSION             (1U)

#define VIBNET_MESSAGE_SENSOR_FIFO          (0x10U)

#define VIBNET_PROTOCOL_HEADER_SIZE         (20U)
#define VIBNET_PROTOCOL_CRC_SIZE            (4U)
#define VIBNET_PROTOCOL_MAX_PAYLOAD_SIZE    (288U)
#define VIBNET_PROTOCOL_MAX_FRAME_SIZE      \
    (VIBNET_PROTOCOL_HEADER_SIZE +          \
     VIBNET_PROTOCOL_MAX_PAYLOAD_SIZE +     \
     VIBNET_PROTOCOL_CRC_SIZE)

typedef struct
{
    uint32_t queued_blocks;
    uint32_t dropped_blocks;
    uint32_t transmitted_blocks;
    uint32_t uart_errors;
} VibnetStreamStats_t;

#ifdef __cplusplus
}
#endif

#endif /* VIBNET_PROTOCOL_H */
