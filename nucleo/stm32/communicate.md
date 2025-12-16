
# UART Communication Specification

**Gateway System: STM32 ↔ ESP32**

---

## 1. Tổng quan giao tiếp UART

Hệ thống gateway gồm hai MCU:

* **STM32**: Thu thập dữ liệu từ các end nodes → Encode → Gửi UART.
* **ESP32**: Nhận dữ liệu từ STM32 → Decode packet → Đẩy dữ liệu lên cloud (Blynk / OTA).

Giao thức UART sử dụng **protocol cố định** gồm 3 trường:
**Length (2 bytes) → Data (8 bytes) → CRC (2 bytes)**

---

# 2. Định dạng Packet UART

Packet UART có độ dài cố định:

```
+---------+------------+---------+
| Length  |   Data     |   CRC   |
| 2 bytes |  8 bytes   | 2 bytes |
+---------+------------+---------+
```

### • 2.1. Length (2 bytes)

* Biểu diễn số bytes thực tế của trường **Data**.
* Với Data cố định 8 bytes → Length luôn = `0x0008`.
* Định dạng Little-endian:

  * Byte thấp trước → Byte cao sau.

### • 2.2. Data (8 bytes)

Dữ liệu truyền đi, tùy theo loại sensor hoặc command:

* Index 0: Loại sensor / message type
* Index 1-7: Payload theo từng sensor (DHT11, MQ2, Light, RFID…)

### • 2.3. CRC (2 bytes)

* CRC16-CCITT (0x1021, seed 0xFFFF)
* CRC được tính trên:

```
CRC = CRC16( Length[0:2] + Data[0:8] )
```

* Không tính CRC cho trường CRC chính nó.

---

# 3. Mã lỗi chung cho cả STM32 và ESP32

```c
typedef enum {
    UART_DECODE_PASSED             = 0,
    UART_DECODE_ERR_INVALID_TYPE   = 1,
    UART_DECODE_ERR_INVALID_LENGTH = 2,
    UART_DECODE_ERR_INVALID_PAYLOAD= 3,
    UART_DECODE_ERR_INVALID_CRC    = 4,
    UART_DECODE_ERR_INVALID_ENDCODE= 5,
    UART_DECODE_ERR_INVALID_TAIL   = 6,
    UART_DECODE_FAILED             = 7,
} uart_error_t;
```

---

# 4. Phần STM32 — UART Encoder

STM32 đóng vai trò **producer** → mã hóa và gửi packet.

## 4.1. Nhiệm vụ STM32

1. Đọc dữ liệu từ các end nodes:

   * **DHT11 (nhiệt độ và độ ẩm)**
   * **MQ2 (gas concentration)**
   * **Light sensor**
   * **RFID module**
2. Build packet:

   * Set Length = 8 bytes
   * Encode data 8-byte payload
   * Tính CRC16
3. Gửi qua UART sang ESP32

---

## 4.2. Quy trình Encode Packet

```
+--------(1) Build Data Payload-------+
| sensor_id | payload[7 bytes]        |
+-------------------------------------+

+--------(2) Set Length = 0x0008------+ 
+--------(3) Append Data---------------+
+--------(4) Compute CRC---------------+
CRC = CRC16(Length + Data)

+--------(5) UART Transmit-------------+
HAL_UART_Transmit_IT(&huart, packet, sizeof(packet));
```

---

# 5. Phía ESP32 — UART Decoder

ESP32 đóng vai trò **consumer**, chịu trách nhiệm nhận và xử lý gói tin.

## 5.1. Nhiệm vụ ESP32

1. Nhận dữ liệu UART từ STM32 theo từng byte.
2. Dùng **Module Decoder (FSM)** để:

   * Kiểm tra Length
   * Thu thập đủ 8 bytes Data
   * Tính CRC và so sánh
   * Trả về *OK* hoặc *mã lỗi*
3. Sau khi decode đúng:

   * Parse payload theo từng sensor
   * Đẩy dữ liệu lên **Cloud – Blynk**
4. (Optional)

   * ESP32 gửi command OTA về STM32
   * Hoặc tự OTA firmware (ESP32 OTA)

---

## 5.2. Quy trình Decode Packet

FSM ví dụ:

```
WAIT_FOR_LENGTH_LSB
WAIT_FOR_LENGTH_MSB
READ_DATA (8 bytes)
READ_CRC_LSB
READ_CRC_MSB
VALIDATE_CRC
DISPATCH_MESSAGE
```

### Kiểm tra CRC

```
if (calculated_crc != packet_crc)
    → UART_DECODE_ERR_INVALID_CRC
```

---

# 6. Chu trình truyền dữ liệu tổng quan

## 6.1. From Sensor → Gateway:

```
DHT11 / MQ2 / Light / RFID
          |
          v
        STM32
   [Encode Packet]
          |
          v
        UART
          |
          v
        ESP32
   [Decode Packet]
          |
          v
         Cloud (Blynk)
```

## 6.2. Optional: OTA

```
Cloud (OTA Server)
        |
        ↓
      ESP32
        |
   UART Command
        |
        ↓
     STM32 OTA
```

---

# 7. Tính nhất quán giữa hai phía

| Thành phần     | STM32      | ESP32               |
| -------------- | ---------- | ------------------- |
| Length (2B)    | ✔          | ✔                   |
| Data (8B)      | ✔          | ✔                   |
| CRC16 (2B)     | ✔ (encode) | ✔ (decode + verify) |
| Mã lỗi         | ✔          | ✔                   |
| Module Encoder | ✔          | ✖                   |
| Module Decoder | ✖          | ✔                   |
| Đẩy data cloud | ✖          | ✔                   |
| OTA            | Optional   | Optional            |

---

# 8. Gợi ý cấu trúc thư mục dự án

```
/uart_protocol/
    encoder_stm32/
        uart_encoder.c
        uart_encoder.h
    decoder_esp32/
        uart_decoder.c
        uart_decoder.h
    common/
        uart_defs.h
        crc16.c
        crc16.h
        uart_errors.h
```
