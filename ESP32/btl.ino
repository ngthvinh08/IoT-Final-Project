/**
 * PROJECT: UART GATEWAY RECEIVER (ESP32)
 * PROTOCOL: Length(2) + Data(8) + CRC(2)
 * Hardware: ESP32 (TX=17, RX=16)
 */

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

// --- Cấu hình Pin ---
#define RX_PIN 16
#define TX_PIN 17

// --- Định nghĩa Protocol ---
#define FIXED_DATA_LEN      8
typedef enum {
    MSG_TYPE_DHT11  = 0x01,
    MSG_TYPE_MQ2    = 0x02,
    MSG_TYPE_RFID   = 0x04
} msg_type_t;

// Mã lỗi
typedef enum {
    UART_DECODE_PASSED              = 0,
    UART_DECODE_ERR_INVALID_CRC     = 4,
    UART_DECODE_WAITING             = 255
} uart_error_t;

// Struct kết quả
typedef struct {
    uint8_t  type;
    uint8_t  payload[7];
    uint16_t crc;
} decoded_packet_t;

// --- CRC Calculate ---
uint16_t calc_crc16(uint16_t crc, uint8_t data) {
    data ^= (crc >> 8) & 0xFF;
    crc <<= 8;
    for (int i = 0; i < 8; i++) {
        if (data & 0x80) data = (data << 1) ^ 0x1021;
        else data <<= 1;
    }
    crc ^= data;
    return crc;
}

// --- DECODER FSM ---
typedef enum {
    WAIT_LEN_LSB,
    WAIT_LEN_MSB,
    READ_DATA,
    READ_CRC_LSB,
    READ_CRC_MSB
} decoder_state_t;

typedef struct {
    decoder_state_t state;
    uint8_t         buffer[FIXED_DATA_LEN];
    uint8_t         buffer_idx;
    uint16_t        calc_crc;
    uint16_t        recv_crc;
    decoded_packet_t output_packet;
} uart_decoder_t;

uart_decoder_t my_decoder;

void decoder_init() {
    my_decoder.state = WAIT_LEN_LSB;
    my_decoder.calc_crc = 0xFFFF;
}

uart_error_t decode_fsm(uint8_t byte) {
    switch (my_decoder.state) {
        case WAIT_LEN_LSB:
            if (byte == 0x08) {
                my_decoder.calc_crc = 0xFFFF;
                my_decoder.calc_crc = calc_crc16(my_decoder.calc_crc, byte);
                my_decoder.state = WAIT_LEN_MSB;
            }
            break;

        case WAIT_LEN_MSB:
            if (byte == 0x00) {
                my_decoder.calc_crc = calc_crc16(my_decoder.calc_crc, byte);
                my_decoder.state = READ_DATA;
                my_decoder.buffer_idx = 0;
            } else {
                my_decoder.state = WAIT_LEN_LSB; 
                // Retry fast sync
                if(byte == 0x08) {
                     my_decoder.calc_crc = 0xFFFF;
                     my_decoder.calc_crc = calc_crc16(my_decoder.calc_crc, byte);
                     my_decoder.state = WAIT_LEN_MSB;
                }
            }
            break;

        case READ_DATA:
            my_decoder.buffer[my_decoder.buffer_idx++] = byte;
            my_decoder.calc_crc = calc_crc16(my_decoder.calc_crc, byte);

            if (my_decoder.buffer_idx >= FIXED_DATA_LEN) {
                my_decoder.state = READ_CRC_LSB;
            }
            break;

        case READ_CRC_LSB:
            my_decoder.recv_crc = (uint16_t)byte;
            my_decoder.state = READ_CRC_MSB;
            break;

        case READ_CRC_MSB:
            my_decoder.recv_crc |= (uint16_t)byte << 8;
            my_decoder.state = WAIT_LEN_LSB; // Reset state

            if (my_decoder.calc_crc == my_decoder.recv_crc) {
                my_decoder.output_packet.type = my_decoder.buffer[0];
                memcpy(my_decoder.output_packet.payload, &my_decoder.buffer[1], 7);
                my_decoder.output_packet.crc = my_decoder.recv_crc;
                return UART_DECODE_PASSED;
            } else {
                return UART_DECODE_ERR_INVALID_CRC;
            }
            break;
    }
    return UART_DECODE_WAITING;
}

// --- Main Loop ---
void setup() {
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
    decoder_init();
    Serial.println("RECEIVER Gateway Started...");
}

void loop() {
    while (Serial2.available()) {
        uint8_t byte = Serial2.read();
        uart_error_t status = decode_fsm(byte);

        if (status == UART_DECODE_PASSED) {
            Serial.println("\n-----------------------------");
            Serial.print("Packet Received! Type: ");
            
            decoded_packet_t* pkt = &my_decoder.output_packet;
            
            switch (pkt->type) {
                case MSG_TYPE_DHT11:
                    Serial.printf("DHT11 | Temp: %d C, Hum: %d %%\n", pkt->payload[0], pkt->payload[1]);
                    break;
                case MSG_TYPE_MQ2: {
                    uint16_t gas = pkt->payload[0] | (pkt->payload[1] << 8);
                    Serial.printf("MQ2   | Gas Level: %d\n", gas);
                    break;
                }
                case MSG_TYPE_RFID:
                    Serial.print("RFID  | UID: ");
                    for(int i=0; i<4; i++) Serial.printf("%02X ", pkt->payload[i]);
                    Serial.println();
                    break;
                default:
                    Serial.printf("UNKNOWN (0x%02X)\n", pkt->type);
            }
        } 
        else if (status == UART_DECODE_ERR_INVALID_CRC) {
            Serial.println("[ERROR] CRC Mismatch!");
        }
    }
}