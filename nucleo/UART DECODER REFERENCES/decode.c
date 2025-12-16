#include "uart_comm.h"
#include "rsi_debug.h"
#include "decode.h"
#include "crc16.h"
#include "string.h"

#define PRINT_PAYLOAD_ENABLE 0

// TODO: Define DECODE_LOG module
// TODO: Implement error states
// TODO: Clean code 
//TODO: Need packet_type_validate no more in decode
//TODO: Test fail cases

// ==================== Initialization of the decoder =======================
bool fsm_decoder_init(uart_fsm_decoder_t *decoder) {

    if (!decoder) {
        DEBUGOUT ("Decoder pointer is NULL\n");
        return false; // check NULL
    }

    decoder->state = WAIT_FOR_HEADER;

    memset(&decoder->packet, 0, sizeof(uart_packet_t));
    memset(decoder->packet_temp_buffer, 0, sizeof(decoder->packet_temp_buffer));
    decoder->packet_temp_buffer_index = 0;

    decoder->calculated_crc = 0;
    decoder->received_crc = 0;

    decoder->total_packets_parsed = 0;
    decoder->packets_accepted = 0;
    decoder->packets_rejected = 0;
    decoder->bad_checksum_count = 0;
    decoder->bad_length_count = 0;

    DEBUGOUT ("UART FSM Decoder initialized successfully\n");
    return true;
}

// ============================= DEBUG FUNCTIONS =============================
void uart_fsm_print_packet(const uart_packet_t *packet){
    DEBUGOUT("\n--- PACKET DETAILS ---\n");
    DEBUGOUT("SOF: 0x%02X\n", packet->sof);
    DEBUGOUT("Type: 0x%04X\n", packet->type);
    DEBUGOUT("Length: %u\n", packet->length);
    DEBUGOUT("Payload: ");
    for (uint16_t i = 0; i < packet->length; i++) {
        DEBUGOUT("%02X ", packet->payload[i]);
    }
    DEBUGOUT("\nEndcode: 0x%02X\n", packet->endcode);
    DEBUGOUT("CRC: 0x%04X\n", packet->crc);
    DEBUGOUT("Tail: 0x%02X\n", packet->tail);
    DEBUGOUT("----------------------\n");
}
// ============================= Validation functions =======================
bool packet_type_validate(uart_packet_t *pkt) {
    switch (pkt->type) {
        case PKT_TYPE_HELLO:
            break;

        default:
            DEBUGOUT("Packet type is invalid: 0x%04X\n", pkt->type);
            return UART_DECODE_ERR_INVALID_TYPE;
    }
    return true;
}

bool packet_length_validate (uart_packet_t *pkt){
    if (pkt->length == 0 || pkt->length > UART_MAX_PACKET_LEN){
        DEBUGOUT ("Packet length is invalid: %u \n", pkt->length);
        return false;
    }
    // DEBUGOUT("Packet LENGTH: %u bytes\n", pkt->length);
    return true;
}

// ==================================== Decode FSM =====================================
void decode_fsm (uart_fsm_decoder_t *decoder, uint8_t byte){
    switch (decoder->state) {
        case WAIT_FOR_HEADER:{
            
            if (byte == UART_HEADER) {
                decoder->packet.sof = byte;
                // Print header 
                DEBUGOUT ("Received HEADER: 0x%02X\n", byte);

                // Reset CRC accumulator 
                decoder->calculated_crc = CRC16_CCITT_INIT;

                decoder->state = READ_TYPE;
                DEBUGOUT("State: WAIT_FOR_HEADER -> READ_TYPE\n");
            }
            break;
        }

        case READ_TYPE: {
            if (decoder->packet_temp_buffer_index == 0) {
                decoder->packet.type  = (uint16_t)byte;
            } else if (decoder->packet_temp_buffer_index == 1) {
                decoder->packet.type |= (uint16_t)byte << 8;
            }
            // CRC include TYPE
            decoder->calculated_crc = crc16_ccitt_update(decoder->calculated_crc, byte);

            decoder->packet_temp_buffer_index++;


            // Print when both bytes of TYPE are read
            if (decoder->packet_temp_buffer_index == 2) {
                DEBUGOUT("Received PACKET TYPE: 0x%04X\n", decoder->packet.type);

                // Validate type
                if (!packet_type_validate(&decoder->packet)) {
                    DEBUGOUT("Invalid type. Resetting to WAIT_FOR_HEADER\n");
                    decoder->state         = WAIT_FOR_HEADER;
                    decoder->packet_temp_buffer_index = 0;
                    break;
                }

                // Print individual bytes of TYPE
                uint8_t type_lsb =  (uint8_t)(decoder->packet.type & 0xFF);
                uint8_t type_msb =  (uint8_t)((decoder->packet.type >> 8) & 0xFF);
                DEBUGOUT("Received TYPE bytes: LSB=0x%02X, MSB=0x%02X\n", type_lsb, type_msb);
                        
                // State transition
                decoder->state         = READ_LENGTH;
                decoder->packet_temp_buffer_index = 0;
                DEBUGOUT("State: READ_TYPE -> READ_LENGTH\n");
            }
            break;
        }

        case READ_LENGTH: {
            if (decoder->packet_temp_buffer_index == 0) {
                decoder->packet.length = (uint16_t)byte;
            } else if (decoder->packet_temp_buffer_index == 1) {
                decoder->packet.length |= ((uint16_t)byte << 8);
            }

            // CRC include TYPE
            decoder->calculated_crc = crc16_ccitt_update(decoder->calculated_crc, byte);

            decoder->packet_temp_buffer_index++;

            // 2bytes read → validate length
            if (decoder->packet_temp_buffer_index == 2) {

                uint8_t len_LSB = (uint8_t)(decoder->packet.length & 0xFF);
                uint8_t len_MSB = (uint8_t)((decoder->packet.length >> 8) & 0xFF);

                DEBUGOUT("Packet LENGTH raw = %u (0x%04X)\n",  decoder->packet.length, decoder->packet.length);

                DEBUGOUT("Received LENGTH bytes: LSB=0x%02X, MSB=0x%02X\n",
                        len_LSB, len_MSB);

                // Validate length
                if (!packet_length_validate(&decoder->packet)) {
                    decoder->state = WAIT_FOR_HEADER;
                    decoder->bad_length_count++;
                    DEBUGOUT("Invalid length. Resetting to WAIT_FOR_HEADER\n");
                    decoder->packet_temp_buffer_index = 0;
                    break;
                }

                // Transit to READ_PAYLOAD
                decoder->state         = READ_PAYLOAD;
                decoder->packet_temp_buffer_index = 0;
                DEBUGOUT("State: READ_LENGTH -> READ_PAYLOAD\n");
            }

            break;
        }

        // TODO: Handle multiple TLV types in one payload
        case READ_PAYLOAD: {
            // Write byte to payload buffer
            if (decoder->packet_temp_buffer_index < UART_MAX_PACKET_LEN) {
                decoder->packet_temp_buffer[decoder->packet_temp_buffer_index++] = byte;

                // CRC include payload byte
                decoder->calculated_crc = crc16_ccitt_update(decoder->calculated_crc, byte);
            } else {
                DEBUGOUT("Payload buffer overflow. Resetting to WAIT_FOR_HEADER\n");
                decoder->state                       = WAIT_FOR_HEADER;
                decoder->packet_temp_buffer_index    = 0;
                decoder->packets_rejected++;
                break;
            }


            // If received full payload
            if (decoder->packet_temp_buffer_index >= decoder->packet.length) {
                // Copy from temp buffer to packet.payload
                memcpy(decoder->packet.payload,
                    decoder->packet_temp_buffer,
                    decoder->packet.length);
        
                #ifndef PRINT_PAYLOAD_ENABLE
                // Print the full payload
                for (uint16_t i = 0; i < decoder->packet.length; i++) {
                    DEBUGOUT("Payload[%u]: 0x%02X\n", i, decoder->packet.payload[i]);
                }
                #endif

                DEBUGOUT("Full payload received (%u bytes)\n", decoder->packet.length);
                

                // State transition to READ_ENDCODE
                decoder->state                    = READ_ENDCODE;
                decoder->packet_temp_buffer_index = 0;
                DEBUGOUT("State: READ_PAYLOAD -> READ_ENDCODE\n");
            }

            break;
        }

        case READ_ENDCODE: {
            
            if (byte == UART_ENDCODE) {
                decoder->packet.endcode = byte;

                // Print endcode
                DEBUGOUT ("Received ENDCODE: 0x%02X\n", byte);

                // CRC include TYPE
                decoder->calculated_crc = crc16_ccitt_update(decoder->calculated_crc, byte);

                decoder->state = READ_CRC;
                decoder->packet_temp_buffer_index = 0;
                DEBUGOUT("State: READ_ENDCODE -> READ_CRC\n");
            } else {
                decoder->state = WAIT_FOR_HEADER;
                DEBUGOUT("Invalid endcode. Resetting to WAIT_FOR_HEADER\n");
            }
            break;
        }

        case READ_CRC: {
            // Receive 2 bytes of CRC
            if (decoder->packet_temp_buffer_index == 0) {
                decoder->received_crc = (uint16_t)byte;          // LSB
            } else if (decoder->packet_temp_buffer_index == 1) {
                decoder->received_crc |= (uint16_t)byte << 8;    // MSB
            }
            decoder->packet_temp_buffer_index++;

            // When received all 2 bytes of CRC
            if (decoder->packet_temp_buffer_index == UART_CRC_SIZE) {

                DEBUGOUT("Received CRC:    0x%04X\n", decoder->received_crc);
                DEBUGOUT("Calculated CRC:  0x%04X\n", decoder->calculated_crc);

                if (decoder->calculated_crc == decoder->received_crc) {
                    decoder->packet.crc  = decoder->received_crc;
                    decoder->state       = READ_TAIL;
                    DEBUGOUT("CRC OK. State: READ_CRC -> READ_TAIL\n");
                } else {
                    decoder->state = WAIT_FOR_HEADER;
                    decoder->bad_checksum_count++;
                    decoder->packets_rejected++;
                    DEBUGOUT("CRC mismatch. Resetting to WAIT_FOR_HEADER\n");
                }

                // Reset index for next state
                decoder->packet_temp_buffer_index = 0;
            }
            break;
        }


        case READ_TAIL:{
            
            if (byte == UART_TAIL) {
                decoder->packet.tail = byte;
                decoder->total_packets_parsed++;
                decoder->packets_accepted++;
                DEBUGOUT("Packet accepted!\n");
                uart_fsm_print_packet(&decoder->packet);
                decoder->state = WAIT_FOR_HEADER; // Reset for next packet
                decoder->packet_temp_buffer_index = 0;
                DEBUGOUT("State: READ_TAIL -> WAIT_FOR_HEADER\n");
            } else {
                decoder->state = WAIT_FOR_HEADER;
                DEBUGOUT("Invalid tail. Resetting to WAIT_FOR_HEADER\n");
            }
            break;
        }

        default:{
            
            decoder->state = WAIT_FOR_HEADER;
            DEBUGOUT("Unknown state. Resetting to WAIT_FOR_HEADER\n");
            break;
        }
    }
}


