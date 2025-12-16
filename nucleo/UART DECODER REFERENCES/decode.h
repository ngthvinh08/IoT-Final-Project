#ifndef SRC_INC_DECODE_H_
#define SRC_INC_DECODE_H_


// Packet types
typedef enum {
    PKT_TYPE_HELLO         = 0x0001,
    PKT_TYPE_TEST          = 0x0101,
    PKT_TYPE_OTA_REQUEST   = 0x0202,
    PKT_TYPE_OTA_RESPONSE  = 0x0203,
    PKT_TYPE_OTA_DATA      = 0x0204,
    PKT_TYPE_UUID_REQUEST  = 0x0210,
    PKT_TYPE_UUID_RESPONSE = 0x0211,
} uart_packet_type_t;



#define UART_HEADER             0xF0
#define UART_ENDCODE            0xFF
#define UART_TAIL               0x0F
#define UART_MAX_PACKET_LEN     512
#define UART_MAX_PAYLOAD_LEN    256

// Constants for calculating CRC
#define UART_HEADER_SIZE        1
#define UART_TYPE_SIZE          2
#define UART_LENGTH_SIZE        2
#define UART_ENCODE_SIZE        1
#define UART_CRC_SIZE           2
#define UART_TAIL_SIZE          1
#define CRC16_FIXED_LENGTH      (UART_TYPE_SIZE + UART_LENGTH_SIZE  + UART_ENCODE_SIZE)    


// UART Packet structure
typedef struct {
    uint8_t            sof;                             // 1 byte
    uint16_t           type;                            // 2 bytes (uint16_t)
    uint16_t           length;                          // 2 bytes
    uint8_t          payload[UART_MAX_PAYLOAD_LEN];   // L max
    uint8_t            endcode;                         // 1 byte
    uint16_t           crc;                             // 2 bytes
    uint8_t            tail;                            // 1 byte
} uart_packet_t;



// FSM states for UART decoding
typedef enum {
    WAIT_FOR_HEADER,
    READ_TYPE,
    READ_LENGTH,
    READ_PAYLOAD,
    READ_ENDCODE,
    READ_CRC,
    READ_TAIL,
} uart_decode_state_t;

// Decoder structure for Message Queue
typedef struct {
    uart_decode_state_t state;
    uart_packet_t packet;
    uint16_t packet_temp_buffer_index;
    uint8_t packet_temp_buffer[UART_MAX_PACKET_LEN];
    uint16_t payload_rx_len;
    
    // CRC
    uint16_t calculated_crc;
    uint16_t received_crc;

    // Statistics
    uint32_t total_packets_parsed;
    uint32_t packets_accepted;
    uint32_t packets_rejected;
    uint32_t bad_checksum_count;
    uint32_t bad_length_count;
} uart_fsm_decoder_t;

// Decode error codes
typedef enum {
    UART_DECODE_PASSED             = 0,
    UART_DECODE_ERR_INVALID_TYPE   = 1,
    UART_DECODE_ERR_INVALID_LENGTH = 2,
    UART_DECODE_ERR_INVALID_PAYLOAD= 3,
    UART_DECODE_ERR_INVALID_CRC    = 4,
    UART_DECODE_ERR_INVALID_ENDCODE= 5,
    UART_DECODE_ERR_INVALID_TAIL   = 6,
    UART_DECODE_FAILED             = 7,
} uart_decode_error_t;



// Decoder initialization 
bool fsm_decoder_init (uart_fsm_decoder_t *decoder);

// Validation functions
bool packet_type_validate (uart_packet_t *pkt);
bool packet_length_validate (uart_packet_t *pkt);   

// Debug utilities
void uart_fsm_print_packet(const uart_packet_t *packet);

// States machine for UART decoding
void decode_fsm (uart_fsm_decoder_t *decoder, uint8_t byte);

#endif /* SRC_INC_DECODE_H_ */
