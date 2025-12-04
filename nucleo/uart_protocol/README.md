# UART Encoder (Packetizer) Module - Hướng Dẫn

## 📋 Tổng Quan

Module encoder này thực hiện công việc **mã hóa dữ liệu từ các sensor thành packet UART** để gửi đến ESP32.

### Quy trình Encoding:
```
Dữ liệu sensor → Build payload (8 bytes) → Tính CRC16 → Gửi packet qua UART
```

### Format Packet (12 bytes):
```
┌─────────┬──────────┬─────────┐
│ Length  │ Data     │ CRC     │
│ 2 bytes │ 8 bytes  │ 2 bytes │
└─────────┴──────────┴─────────┘
  0x0008    [8 bytes]  [CRC16]
```

---

## 🏗️ Cấu Trúc Module

```
uart_protocol/
├── uart_defs.h          ← Định nghĩa packet, sensor types, error codes
├── crc16.h / crc16.c    ← Hàm tính CRC16-CCITT
├── uart_encoder.h       ← Khai báo hàm encoder
├── uart_encoder.c       ← Triển khai encoder
└── test_encoder.c       ← Unit tests
```

---

## 💡 Cách Sử Dụng

### 1️⃣ Include Headers

```c
#include "uart_encoder.h"  // Includes uart_defs.h và crc16.h automatically
```

### 2️⃣ Encoding DHT11 (Temperature & Humidity)

```c
uart_packet_t packet;
uart_error_t status;

// Encode dữ liệu DHT11: Nhiệt độ = 25°C, Độ ẩm = 60%
status = uart_encode_dht11(25, 60, &packet);

if (status == UART_ENCODE_OK) {
    // Gửi packet qua UART
    HAL_UART_Transmit_IT(&huart, (uint8_t*)&packet, sizeof(uart_packet_t));
}
```

### 3️⃣ Encoding MQ2 (Gas Sensor)

```c
uart_packet_t packet;

// Encode MQ2 sensor: PPM = 500
uart_encode_mq2(500, &packet);

// Gửi
HAL_UART_Transmit_IT(&huart, (uint8_t*)&packet, sizeof(uart_packet_t));
```

### 4️⃣ Encoding Light Sensor

```c
uart_packet_t packet;

// Encode Light sensor: 800 lux
uart_encode_light(800, &packet);

// Gửi
HAL_UART_Transmit_IT(&huart, (uint8_t*)&packet, sizeof(uart_packet_t));
```

### 5️⃣ Encoding RFID

```c
uart_packet_t packet;
uint8_t rfid_id[4] = {0xAB, 0xCD, 0xEF, 0x12};

// Encode RFID card ID
uart_encode_rfid(rfid_id, &packet);

// Gửi
HAL_UART_Transmit_IT(&huart, (uint8_t*)&packet, sizeof(uart_packet_t));
```

### 6️⃣ Generic Encoder (Custom Payload)

```c
uart_packet_t packet;
uint8_t custom_payload[7] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};

// Encode với message type custom
uart_encode_packet(UART_MSG_LIGHT, custom_payload, &packet);
```

---

## 📊 Packet Structure Details

### Length Field (2 bytes, Little-Endian)
```
byte[0] (LSB)  = 0x08
byte[1] (MSB)  = 0x00
→ Value = 0x0008 (8 bytes)
```

### Data Field (8 bytes)
```
byte[0] = Message Type / Sensor ID
          - 0x01: DHT11
          - 0x02: MQ2
          - 0x03: Light
          - 0x04: RFID
          - 0x05: Command

byte[1-7] = Payload (7 bytes)
```

#### Payload Format theo Sensor:

**DHT11:**
```
byte[0] = Temperature (int8_t, -40 to 125°C)
byte[1] = Humidity (0-100%)
byte[2-6] = Reserved
```

**MQ2:**
```
byte[0-1] = PPM (uint16_t, little-endian)
byte[2-6] = Reserved
```

**Light:**
```
byte[0-1] = Lux (uint16_t, little-endian)
byte[2-6] = Reserved
```

**RFID:**
```
byte[0-3] = RFID Card ID (4 bytes)
byte[4-6] = Reserved
```

### CRC Field (2 bytes, Little-Endian)
```
CRC16-CCITT (polynomial 0x1021, seed 0xFFFF)
Tính trên: Length (2 bytes) + Data (8 bytes)
byte[0] = CRC LSB
byte[1] = CRC MSB
```

---

## 🔧 Hàm Chính

### Generic Encoder

```c
uart_error_t uart_encode_packet(
    uint8_t msg_type,              // Sensor ID (UART_MSG_*)
    const uint8_t* payload,        // 7-byte payload
    uart_packet_t* output_packet   // Output packet
);
```

**Return Values:**
- `UART_ENCODE_OK (0)` - Thành công
- `UART_ENCODE_ERR_INVALID_TYPE` - Sensor type không hợp lệ
- `UART_ENCODE_ERR_INVALID_PAYLOAD` - Payload bị lỗi

### Sensor-Specific Encoders

```c
// DHT11 Encoder
uart_error_t uart_encode_dht11(
    int8_t temperature,            // -40 to 125°C
    uint8_t humidity,              // 0-100%
    uart_packet_t* output_packet
);

// MQ2 Encoder
uart_error_t uart_encode_mq2(
    uint16_t ppm,                  // 0-65535
    uart_packet_t* output_packet
);

// Light Sensor Encoder
uart_error_t uart_encode_light(
    uint16_t lux,                  // 0-65535
    uart_packet_t* output_packet
);

// RFID Encoder
uart_error_t uart_encode_rfid(
    const uint8_t* rfid_data,      // 4-byte RFID ID
    uart_packet_t* output_packet
);
```

---

## 🧪 Testing

### Chạy Unit Tests

```c
// Thêm vào main.c (trong while loop hoặc initialization)
#include "test_encoder.c"

int main(void) {
    // STM32 initialization...
    
    // Run tests
    uart_encoder_run_tests();
    
    while(1) {
        // Main loop
    }
}
```

**Test Coverage:**
- ✅ DHT11 encoding (normal + edge cases)
- ✅ MQ2 encoding
- ✅ Light sensor encoding
- ✅ RFID encoding
- ✅ CRC consistency check

---

## 📐 Little-Endian Conversion

**Ví dụ:** Encode PPM = 0x01F4 (500 in decimal)

```
Value: 0x01F4
├─ LSB (byte[0]) = 0xF4
└─ MSB (byte[1]) = 0x01

In packet:
  payload[0] = 0xF4  (500 & 0xFF)
  payload[1] = 0x01  ((500 >> 8) & 0xFF)
```

**Decode (reverse):**
```c
uint16_t ppm = payload[0] | (payload[1] << 8);  // = 0x01F4
```

---

## 🚀 Integration Steps

1. **Thêm files vào project STM32:**
   ```
   Core/Src/
   ├── uart_encoder.c
   ├── crc16.c
   └── main.c (modified)
   
   Core/Inc/
   ├── uart_encoder.h
   ├── uart_defs.h
   ├── crc16.h
   └── main.h
   ```

2. **Update main.c:**
   ```c
   #include "uart_encoder.h"
   
   int main(void) {
       HAL_Init();
       MX_USART1_UART_Init();
       
       uart_packet_t packet;
       
       while(1) {
           // Read DHT11
           int temp = read_dht11_temp();
           int humi = read_dht11_humidity();
           
           // Encode & send
           uart_encode_dht11(temp, humi, &packet);
           HAL_UART_Transmit_IT(&huart1, (uint8_t*)&packet, 12);
           
           HAL_Delay(1000);
       }
   }
   ```

3. **Compile & Flash:**
   ```bash
   make
   make flash
   ```

---

## ❌ Error Handling

```c
uart_packet_t packet;
uart_error_t status = uart_encode_dht11(25, 101, &packet);  // Invalid humidity

if (status != UART_ENCODE_OK) {
    printf("Encode error: %d\n", status);
    // Handle error appropriately
}
```

**Error Codes:**
```c
typedef enum {
    UART_ENCODE_OK                  = 0,
    UART_ENCODE_ERR_INVALID_TYPE    = 1,
    UART_ENCODE_ERR_INVALID_LENGTH  = 2,
    UART_ENCODE_ERR_INVALID_PAYLOAD = 3,
    UART_ENCODE_ERR_BUFFER_OVERFLOW = 4,
    UART_ENCODE_FAILED              = 7,
} uart_error_t;
```

---

## 📝 Ví Dụ Packet Dump

### DHT11: Temp=25°C, Humidity=60%

```
Raw bytes: 08 00 | 01 19 3C 00 00 00 00 00 | E3 CC

Breakdown:
- Length:  08 00 → 0x0008 (8 bytes)
- Data[0]: 01    → UART_MSG_DHT11
- Data[1]: 19    → 25 (temperature in °C)
- Data[2]: 3C    → 60 (humidity %)
- Data[3-7]: 00  → Reserved
- CRC:     E3 CC → CRC16 value
```

### MQ2: PPM=500

```
Raw bytes: 08 00 | 02 F4 01 00 00 00 00 00 | ...

Breakdown:
- Length:  08 00 → 0x0008
- Data[0]: 02    → UART_MSG_MQ2
- Data[1]: F4    → PPM LSB (500 & 0xFF = 0xF4)
- Data[2]: 01    → PPM MSB ((500 >> 8) = 0x01)
- Data[3-7]: 00  → Reserved
- CRC:     ...   → CRC16 value
```

---

## 🔗 Liên Kết Quan Trọng

- **CRC16-CCITT:** Polynomial 0x1021, Initial 0xFFFF
- **Endianness:** Little-endian (LSB first)
- **Baud Rate:** Phải match cấu hình UART STM32 ↔ ESP32
- **Timeout:** Xử lý timeout UART transmit nếu cần

---

## 💭 Q&A

**Q: Tại sao packet luôn có Length = 0x0008?**
A: Vì spec định nghĩa Data field cố định 8 bytes. Nếu muốn linh hoạt hơn, có thể modify Length field dinamically.

**Q: Nếu sensor data lớn hơn 7 bytes thì sao?**
A: Có thể:
1. Split thành nhiều packets
2. Compress dữ liệu
3. Extend Data field (nhưng phải update cùng ESP32 decoder)

**Q: CRC không khớp tại sao?**
A: Kiểm tra:
1. Polynomial 0x1021, seed 0xFFFF đúng chưa
2. CRC tính trên Length + Data (không tính CRC field)
3. Endianness của CRC (LSB first)

---

## 📖 Tài Liệu Tham Khảo

- `communicate.md` - UART Protocol Specification
- `uart_defs.h` - Packet definitions & error codes
- `uart_encoder.h` - Function prototypes
- `crc16.h` - CRC16-CCITT algorithm

---

**Tác giả:** IoT Gateway System
**Ngày:** November 2025
**Version:** 1.0
