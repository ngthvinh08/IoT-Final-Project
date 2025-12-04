#include "uart_encoder.h"
#include <string.h>
#include <uart_driver.h>
#include "stdio.h"


/**
 * @brief Set packet length field (little-endian)
 * @param packet    Pointer to packet
 * @param length    Length value
 */
static void _set_packet_length(uart_packet_t* packet, uint16_t length)
{
    packet->length[0] = (uint8_t)(length & 0xFF);          /* LSB */
    packet->length[1] = (uint8_t)((length >> 8) & 0xFF);   /* MSB */
}

/**
 * @brief Set packet CRC field (little-endian)
 * @param packet    Pointer to packet
 * @param crc       CRC value
 */
static void _set_packet_crc(uart_packet_t* packet, uint16_t crc)
{
    packet->crc[0] = (uint8_t)(crc & 0xFF);          /* LSB */
    packet->crc[1] = (uint8_t)((crc >> 8) & 0xFF);   /* MSB */
}

/**
 * @brief Validate message type
 * @param msg_type  Message type to validate
 * @return          UART_ENCODE_OK if valid, UART_ENCODE_ERR_INVALID_TYPE otherwise
 */
static uart_error_t _validate_msg_type(uint8_t msg_type)
{
    switch (msg_type) {
        case UART_MSG_DHT11:
        case UART_MSG_MQ2:
        case UART_MSG_LIGHT:
        case UART_MSG_RFID:
        case UART_MSG_CMD:
            return UART_ENCODE_OK;
        default:
            return UART_ENCODE_ERR_INVALID_TYPE;
    }
}

/**
 * ============================================================================
 * Core Encoder Functions
 * ============================================================================
 */

uart_error_t uart_encode_packet(uint8_t msg_type, const uint8_t* payload, uart_packet_t* output_packet)
{
    /* Validate inputs */
    if (output_packet == NULL) {
        return UART_ENCODE_ERR_INVALID_PAYLOAD;
    }

    if (payload == NULL) {
        return UART_ENCODE_ERR_INVALID_PAYLOAD;
    }

    /* Validate message type */
    uart_error_t err = _validate_msg_type(msg_type);
    if (err != UART_ENCODE_OK) {
        return err;
    }

    /* ========== Step 1: Set Length field ========== */
    _set_packet_length(output_packet, UART_DATA_LENGTH_VALUE);

    /* ========== Step 2: Build Data field ========== */
    output_packet->data[0] = msg_type;  /* Index 0: Message type */
    memcpy(&output_packet->data[1], payload, 7);  /* Index 1-7: Payload (7 bytes) */

    /* ========== Step 3: Calculate CRC ========== */
    /* CRC is calculated over: Length field (2 bytes) + Data field (8 bytes) = 10 bytes total */
    uint8_t crc_input[UART_PACKET_LENGTH_SIZE + UART_PACKET_DATA_SIZE];
    memcpy(&crc_input[0], output_packet->length, UART_PACKET_LENGTH_SIZE);
    memcpy(&crc_input[UART_PACKET_LENGTH_SIZE], output_packet->data, UART_PACKET_DATA_SIZE);

    uint16_t crc = crc16_calculate(crc_input, sizeof(crc_input));

    /* ========== Step 4: Set CRC field ========== */
    _set_packet_crc(output_packet, crc);

    return UART_ENCODE_OK;
}

/**
 * ============================================================================
 * Sensor-Specific Encoder Functions
 * ============================================================================
 */

uart_error_t uart_encode_dht11(int8_t temperature, uint8_t humidity, uart_packet_t* output_packet)
{
    /* Validate inputs */
    if (output_packet == NULL) {
        return UART_ENCODE_ERR_INVALID_PAYLOAD;
    }

    /* Optional: Validate DHT11 ranges */
    if (temperature < -40 || temperature > 125) {
        return UART_ENCODE_ERR_INVALID_PAYLOAD;
    }

    if (humidity > 100) {
        return UART_ENCODE_ERR_INVALID_PAYLOAD;
    }

    /* Build 7-byte payload */
    uint8_t payload[7] = {0};
    payload[0] = (uint8_t)temperature;  /* Temperature */
    payload[1] = humidity;               /* Humidity */
    /* Index 2-6: Reserved / padding */

    /* Encode using generic function */
    return uart_encode_packet(UART_MSG_DHT11, payload, output_packet);
}

uart_error_t uart_encode_mq2(uint16_t ppm, uart_packet_t* output_packet)
{
    /* Validate inputs */
    if (output_packet == NULL) {
        return UART_ENCODE_ERR_INVALID_PAYLOAD;
    }

    /* Build 7-byte payload */
    uint8_t payload[7] = {0};
    payload[0] = (uint8_t)(ppm & 0xFF);          /* PPM LSB */
    payload[1] = (uint8_t)((ppm >> 8) & 0xFF);   /* PPM MSB */
    /* Index 2-6: Reserved / padding */

    /* Encode using generic function */
    return uart_encode_packet(UART_MSG_MQ2, payload, output_packet);
}

uart_error_t uart_encode_light(uint16_t lux, uart_packet_t* output_packet)
{
    /* Validate inputs */
    if (output_packet == NULL) {
        return UART_ENCODE_ERR_INVALID_PAYLOAD;
    }

    /* Build 7-byte payload */
    uint8_t payload[7] = {0};
    payload[0] = (uint8_t)(lux & 0xFF);          /* Lux LSB */
    payload[1] = (uint8_t)((lux >> 8) & 0xFF);   /* Lux MSB */
    /* Index 2-6: Reserved / padding */

    /* Encode using generic function */
    return uart_encode_packet(UART_MSG_LIGHT, payload, output_packet);
}

uart_error_t uart_encode_rfid(const uint8_t* rfid_data, uart_packet_t* output_packet)
{
    /* Validate inputs */
    if (output_packet == NULL || rfid_data == NULL) {
        return UART_ENCODE_ERR_INVALID_PAYLOAD;
    }

    /* Build 7-byte payload */
    uint8_t payload[7] = {0};
    memcpy(&payload[0], rfid_data, 4);  /* RFID card ID (4 bytes) */
    /* Index 4-6: Reserved / padding */

    /* Encode using generic function */
    return uart_encode_packet(UART_MSG_RFID, payload, output_packet);
}

/**
 * ============================================================================
 * High-Level API with UART Transmission
 * ============================================================================
 */

uart_error_t uart_send_packet(uint8_t msg_type, const uint8_t* payload)
{
    uart_packet_t packet;
    uart_error_t err;

    /* Encode packet */
    err = uart_encode_packet(msg_type, payload, &packet);
    if (err != UART_ENCODE_OK) {
        return err;
    }

    /* Send via UART driver */
    uart_send((uint8_t*)&packet, UART_PACKET_TOTAL_SIZE);

    return UART_ENCODE_OK;
}

uart_error_t uart_send_dht11_packet(int8_t temperature, uint8_t humidity)
{
    uart_packet_t packet;
    uart_error_t err;

    /* Encode DHT11 packet */
    err = uart_encode_dht11(temperature, humidity, &packet);
    if (err != UART_ENCODE_OK) {
        return err;
    }

    /* Send via UART driver */
    uart_send((uint8_t*)&packet, UART_PACKET_TOTAL_SIZE);

    return UART_ENCODE_OK;
}

uart_error_t uart_send_mq2_packet(uint16_t LPG, uint16_t CO, uint16_t SMOKE)
{
    uart_packet_t packet;
    uart_error_t err;

    /* Build 7-byte payload */
    uint8_t payload[7] = {0};
    payload[0] = (uint8_t)(LPG & 0xFF);          /* LPG LSB */
    payload[1] = (uint8_t)((LPG >> 8) & 0xFF);   /* LPG MSB */
    payload[2] = (uint8_t)(CO & 0xFF);           /* CO LSB */
    payload[3] = (uint8_t)((CO >> 8) & 0xFF);    /* CO MSB */
    payload[4] = (uint8_t)(SMOKE & 0xFF);        /* SMOKE LSB */
    payload[5] = (uint8_t)((SMOKE >> 8) & 0xFF); /* SMOKE MSB */
    /* Index 6: Reserved / padding */

    /* Encode MQ2 packet */
    err = uart_encode_packet(UART_MSG_MQ2, payload, &packet);
    if (err != UART_ENCODE_OK) {
        return err;
    }

    /* Send via UART driver */
    uart_send((uint8_t*)&packet, UART_PACKET_TOTAL_SIZE);

    return UART_ENCODE_OK;
}

uart_error_t uart_send_light_packet(uint16_t lux)
{
    uart_packet_t packet;
    uart_error_t err;

    /* Encode Light packet */
    err = uart_encode_light(lux, &packet);
    if (err != UART_ENCODE_OK) {
        return err;
    }

    /* Send via UART driver */
    uart_send((uint8_t*)&packet, UART_PACKET_TOTAL_SIZE);

    return UART_ENCODE_OK;
}

uart_error_t uart_send_rfid_packet(const uint8_t* rfid_data)
{
    uart_packet_t packet;
    uart_error_t err;

    /* Encode RFID packet */
    err = uart_encode_rfid(rfid_data, &packet);
    if (err != UART_ENCODE_OK) {
        return err;
    }

    /* Send via UART driver */
    uart_send((uint8_t*)&packet, UART_PACKET_TOTAL_SIZE);

    return UART_ENCODE_OK;
}

/**
 * @brief Send formatted text instead of binary (for debugging)
 */
uart_error_t uart_send_dht11_text(int8_t temperature, uint8_t humidity)
{
    char buffer[50];
    int len = sprintf(buffer, "DHT11: T=%d°C, H=%d%%\r\n", temperature, humidity);
    uart_send((uint8_t*)buffer, len);
    return UART_ENCODE_OK;
}

uart_error_t uart_send_mq2_text(uint16_t ppm)
{
    char buffer[50];
    int len = sprintf(buffer, "MQ2: %d PPM\r\n", ppm);
    uart_send((uint8_t*)buffer, len);
    return UART_ENCODE_OK;
}

uart_error_t uart_send_light_text(uint16_t lux)
{
    char buffer[50];
    int len = sprintf(buffer, "LIGHT: %d lux\r\n", lux);
    uart_send((uint8_t*)buffer, len);
    return UART_ENCODE_OK;
}

uart_error_t uart_send_rfid_text(const uint8_t* rfid_data)
{
    char buffer[50];
    int len = sprintf(buffer, "RFID: %02X %02X %02X %02X\r\n", 
                     rfid_data[0], rfid_data[1], rfid_data[2], rfid_data[3]);
    uart_send((uint8_t*)buffer, len);
    return UART_ENCODE_OK;
}
