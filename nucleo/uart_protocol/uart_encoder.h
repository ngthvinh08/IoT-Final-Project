#ifndef __UART_ENCODER_H
#define __UART_ENCODER_H

#include "uart_defs.h"
#include "crc16.h"

/**
 * ============================================================================
 * UART Encoder (Packetizer) Module
 * ============================================================================
 * 
 * Công dụng:
 * - Mã hóa dữ liệu từ các sensors (DHT11, MQ2, Light, RFID...)
 * - Tạo packet theo format cố định: Length(2B) + Data(8B) + CRC(2B)
 * - Gửi packet qua UART
 * 
 * Quy trình:
 * 1. Build dữ liệu 8-byte payload (msg_type + 7 bytes dữ liệu)
 * 2. Set Length = 0x0008
 * 3. Tính CRC16 trên (Length + Data)
 * 4. Trả về packet hoặc gửi trực tiếp qua UART
 */

/**
 * @brief Encode a UART packet from message type and payload
 * 
 * @param msg_type      Message type / Sensor ID (UART_MSG_*)
 * @param payload       Pointer to 7-byte payload data
 * @param output_packet Pointer to output buffer (should be at least 12 bytes)
 * @return              Error code (UART_ENCODE_OK if success)
 * 
 * Example:
 *   uint8_t payload[7] = {0x22, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00};  // DHT11: temp=34°C, humidity=85%
 *   uart_packet_t packet;
 *   uart_error_t status = uart_encode_packet(UART_MSG_DHT11, payload, &packet);
 */
uart_error_t uart_encode_packet(uint8_t msg_type, const uint8_t* payload, uart_packet_t* output_packet);

/**
 * @brief Encode DHT11 (Temperature & Humidity) packet
 * 
 * @param temperature   Temperature value (e.g., 25 for 25°C)
 * @param humidity      Humidity value (e.g., 60 for 60%)
 * @param output_packet Pointer to output packet buffer
 * @return              Error code
 * 
 * Payload format:
 *   - Index 1: Temperature (int8_t, -40 to 125)
 *   - Index 2: Humidity (0-100)
 *   - Index 3-7: Reserved / padding
 */
uart_error_t uart_encode_dht11(int8_t temperature, uint8_t humidity, uart_packet_t* output_packet);

/**
 * @brief Encode MQ2 (Gas sensor) packet
 * 
 * @param ppm           PPM value (parts per million, 0-1000+)
 * @param output_packet Pointer to output packet buffer
 * @return              Error code
 * 
 * Payload format:
 *   - Index 1-2: PPM value (uint16_t, little-endian)
 *   - Index 3-7: Reserved / padding
 */
uart_error_t uart_encode_mq2(uint16_t ppm, uart_packet_t* output_packet);

/**
 * @brief Encode Light Sensor packet
 * 
 * @param lux           Light intensity (lux, 0-65535)
 * @param output_packet Pointer to output packet buffer
 * @return              Error code
 * 
 * Payload format:
 *   - Index 1-2: Lux value (uint16_t, little-endian)
 *   - Index 3-7: Reserved / padding
 */
uart_error_t uart_encode_light(uint16_t lux, uart_packet_t* output_packet);

/**
 * @brief Encode RFID packet
 * 
 * @param rfid_data     Pointer to RFID card ID (4 bytes)
 * @param output_packet Pointer to output packet buffer
 * @return              Error code
 * 
 * Payload format:
 *   - Index 1-4: RFID card ID (4 bytes)
 *   - Index 5-7: Reserved / padding
 */
uart_error_t uart_encode_rfid(const uint8_t* rfid_data, uart_packet_t* output_packet);

/**
 * ============================================================================
 * High-Level API with UART Transmission
 * ============================================================================
 * 
 * Các hàm này kết hợp encode + transmit thành một bước
 * Yêu cầu: driver.h phải cung cấp hàm uart_send()
 */

/**
 * @brief Encode and send DHT11 packet over UART
 * 
 * @param temperature   Temperature value
 * @param humidity      Humidity value
 * @return              Error code
 * 
 * Example:
 *   uart_send_dht11_packet(25, 60);  // Auto encode & send
 */
uart_error_t uart_send_dht11_packet(int8_t temperature, uint8_t humidity);

/**
 * @brief Encode and send MQ2 packet over UART
 */
uart_error_t uart_send_mq2_packet (uint16_t LPG, uint16_t CO, uint16_t SMOKE);

/**
 * @brief Encode and send Light packet over UART
 */
uart_error_t uart_send_light_packet(uint16_t lux);

/**
 * @brief Encode and send RFID packet over UART
 */
uart_error_t uart_send_rfid_packet(const uint8_t* rfid_data);

/**
 * @brief Generic encode and send function
 * 
 * @param msg_type      Message type (UART_MSG_*)
 * @param payload       7-byte payload
 * @return              Error code
 */
uart_error_t uart_send_packet(uint8_t msg_type, const uint8_t* payload);


uart_error_t uart_send_mq2_text(uint16_t ppm);
uart_error_t uart_send_dht11_text(int8_t temperature, uint8_t humidity);
uart_error_t uart_send_light_text(uint16_t lux);
uart_error_t uart_send_rfid_text(const uint8_t* rfid_data);

#endif /* __UART_ENCODER_H */
