#ifndef __UART_DEFS_H
#define __UART_DEFS_H

#include <stdint.h>
#include <stddef.h>


/* ========== Packet Constants ========== */
#define UART_PACKET_LENGTH_SIZE     2   /* Length field size in bytes */
#define UART_PACKET_DATA_SIZE       8   /* Data field size in bytes */
#define UART_PACKET_CRC_SIZE        2   /* CRC field size in bytes */
#define UART_PACKET_TOTAL_SIZE      (UART_PACKET_LENGTH_SIZE + UART_PACKET_DATA_SIZE + UART_PACKET_CRC_SIZE)  /* Total: 12 bytes */

/* Fixed length value (always 0x0008 for 8 bytes of data) */
#define UART_DATA_LENGTH_VALUE      UART_PACKET_DATA_SIZE

/* ========== Message Types / Sensor IDs ========== */
typedef enum {
    UART_MSG_DHT11      = 0x01,    /* Temperature & Humidity sensor */
    UART_MSG_MQ2        = 0x02,    /* Gas concentration sensor */
    UART_MSG_LIGHT      = 0x03,    /* Light sensor */
    UART_MSG_RFID       = 0x04,    /* RFID module */
    UART_MSG_CMD        = 0x05,    /* Command from ESP32 */
} uart_message_type_t;

/* ========== Error Codes ========== */
typedef enum {
    UART_ENCODE_OK                  = 0,
    UART_ENCODE_ERR_INVALID_TYPE    = 1,
    UART_ENCODE_ERR_INVALID_LENGTH  = 2,
    UART_ENCODE_ERR_INVALID_PAYLOAD = 3,
    UART_ENCODE_ERR_BUFFER_OVERFLOW = 4,
    UART_ENCODE_FAILED              = 7,
} uart_error_t;

/* ========== Packet Structure ========== */
typedef struct {
    uint8_t length[2];      /* Length field (2 bytes, little-endian) */
    uint8_t data[8];        /* Data payload (8 bytes) */
    uint8_t crc[2];         /* CRC16-CCITT (2 bytes, little-endian) */
} fuart_packet_t;

/* ========== Message Payload Structure (8 bytes) ========== */
typedef struct {
    uint8_t msg_type;       /* Index 0: Message type (sensor ID) */
    uint8_t payload[7];     /* Index 1-7: Payload data */
} uart_message_t;

#endif /* __UART_DEFS_H */
