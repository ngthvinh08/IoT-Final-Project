#include "uart_encoder.h"
#include <stdio.h>

/**
 * ============================================================================
 * Unit Tests for UART Encoder
 * ============================================================================
 * 
 * Hướng dẫn sử dụng:
 * - Thêm file này vào project
 * - Tạo một main() test function
 * - Chạy trên STM32 hoặc simulator
 */

/**
 * @brief Helper function to print hex bytes
 */
static void print_packet_hex(const uart_packet_t* packet)
{
    printf("Packet hex: ");
    for (int i = 0; i < 2; i++) printf("%02X ", packet->length[i]);
    for (int i = 0; i < 8; i++) printf("%02X ", packet->data[i]);
    for (int i = 0; i < 2; i++) printf("%02X ", packet->crc[i]);
    printf("\n");
}

/**
 * @brief Test DHT11 encoding
 */
void test_encode_dht11(void)
{
    printf("\n=== Test DHT11 Encoding ===\n");
    
    uart_packet_t packet;
    uart_error_t err;

    /* Test case 1: Normal values */
    printf("Test 1: Temp=25°C, Humidity=60%%\n");
    err = uart_encode_dht11(25, 60, &packet);
    printf("Status: %d (0=OK)\n", err);
    printf("Length: 0x%02X%02X (should be 0x0008)\n", packet.length[1], packet.length[0]);
    printf("Data[0] (Type): 0x%02X (should be 0x01)\n", packet.data[0]);
    printf("Data[1] (Temp): %d\n", (int8_t)packet.data[1]);
    printf("Data[2] (Humidity): %d\n", packet.data[2]);
    print_packet_hex(&packet);

    /* Test case 2: Edge values */
    printf("\nTest 2: Temp=-40°C, Humidity=100%%\n");
    err = uart_encode_dht11(-40, 100, &packet);
    printf("Status: %d\n", err);
    printf("Data[1] (Temp): %d\n", (int8_t)packet.data[1]);
    printf("Data[2] (Humidity): %d\n", packet.data[2]);
    print_packet_hex(&packet);

    /* Test case 3: Invalid humidity (> 100) */
    printf("\nTest 3: Invalid humidity (101%%)\n");
    err = uart_encode_dht11(25, 101, &packet);
    printf("Status: %d (should be non-zero error)\n", err);
}

/**
 * @brief Test MQ2 encoding
 */
void test_encode_mq2(void)
{
    printf("\n=== Test MQ2 Encoding ===\n");

    uart_packet_t packet;
    uart_error_t err;

    /* Test case 1: Normal PPM value */
    printf("Test 1: PPM=500\n");
    err = uart_encode_mq2(500, &packet);
    printf("Status: %d\n", err);
    printf("Data[0] (Type): 0x%02X (should be 0x02)\n", packet.data[0]);
    uint16_t ppm = packet.data[1] | (packet.data[2] << 8);  /* Read back PPM */
    printf("Data[1-2] (PPM): %d\n", ppm);
    print_packet_hex(&packet);

    /* Test case 2: Large PPM value */
    printf("\nTest 2: PPM=1000\n");
    err = uart_encode_mq2(1000, &packet);
    printf("Status: %d\n", err);
    ppm = packet.data[1] | (packet.data[2] << 8);
    printf("Data[1-2] (PPM): %d\n", ppm);
    print_packet_hex(&packet);
}

/**
 * @brief Test Light Sensor encoding
 */
void test_encode_light(void)
{
    printf("\n=== Test Light Sensor Encoding ===\n");

    uart_packet_t packet;
    uart_error_t err;

    /* Test case 1: Normal lux value */
    printf("Test 1: Lux=800\n");
    err = uart_encode_light(800, &packet);
    printf("Status: %d\n", err);
    printf("Data[0] (Type): 0x%02X (should be 0x03)\n", packet.data[0]);
    uint16_t lux = packet.data[1] | (packet.data[2] << 8);
    printf("Data[1-2] (Lux): %d\n", lux);
    print_packet_hex(&packet);

    /* Test case 2: Max lux value */
    printf("\nTest 2: Lux=65535\n");
    err = uart_encode_light(65535, &packet);
    printf("Status: %d\n", err);
    lux = packet.data[1] | (packet.data[2] << 8);
    printf("Data[1-2] (Lux): %d\n", lux);
}

/**
 * @brief Test RFID encoding
 */
void test_encode_rfid(void)
{
    printf("\n=== Test RFID Encoding ===\n");

    uart_packet_t packet;
    uart_error_t err;

    uint8_t rfid_id[4] = {0xAB, 0xCD, 0xEF, 0x12};
    
    printf("Test 1: RFID ID = AB CD EF 12\n");
    err = uart_encode_rfid(rfid_id, &packet);
    printf("Status: %d\n", err);
    printf("Data[0] (Type): 0x%02X (should be 0x04)\n", packet.data[0]);
    printf("Data[1-4] (RFID): %02X %02X %02X %02X\n", 
           packet.data[1], packet.data[2], packet.data[3], packet.data[4]);
    print_packet_hex(&packet);
}

/**
 * @brief Test CRC calculation consistency
 */
void test_crc_consistency(void)
{
    printf("\n=== Test CRC Consistency ===\n");

    uart_packet_t packet1, packet2;

    /* Encode same data twice */
    uart_encode_dht11(25, 60, &packet1);
    uart_encode_dht11(25, 60, &packet2);

    printf("CRC1: 0x%02X%02X\n", packet1.crc[1], packet1.crc[0]);
    printf("CRC2: 0x%02X%02X\n", packet2.crc[1], packet2.crc[0]);
    printf("Match: %s\n", (packet1.crc[0] == packet2.crc[0] && packet1.crc[1] == packet2.crc[1]) ? "YES" : "NO");
}

/**
 * @brief Main test runner (for STM32)
 * 
 * Gọi hàm này từ main.c để chạy tests
 */
void uart_encoder_run_tests(void)
{
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║     UART Encoder Unit Tests Started    ║\n");
    printf("╚════════════════════════════════════════╝\n");

    test_encode_dht11();
    test_encode_mq2();
    test_encode_light();
    test_encode_rfid();
    test_crc_consistency();

    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║      UART Encoder Unit Tests Done      ║\n");
    printf("╚════════════════════════════════════════╝\n");
}
