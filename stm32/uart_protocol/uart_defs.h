#ifndef __UART_DEFS_H
#define __UART_DEFS_H

#include <stdint.h>
#include <stddef.h>


/* ========== Packet Constants ========== */
#define UART_PACKET_LENGTH_SIZE     2   
#define UART_PACKET_DATA_SIZE       8   
#define UART_PACKET_CRC_SIZE        2   
#define UART_PACKET_TOTAL_SIZE      (UART_PACKET_LENGTH_SIZE + UART_PACKET_DATA_SIZE + UART_PACKET_CRC_SIZE)  
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
    uint8_t length[UART_PACKET_LENGTH_SIZE];      
    uint8_t data[UART_PACKET_DATA_SIZE];        
    uint8_t crc[UART_PACKET_CRC_SIZE];         
} uart_packet_t;

/* ========== Message Payload Structure (8 bytes) ========== */
typedef struct {
    uint8_t msg_type;       
    uint8_t payload[7];     // Index 1-7: Payload data 
} uart_message_t;

#endif /* __UART_DEFS_H */
