#define BLYNK_TEMPLATE_ID        "TMPL6qE8u9pAl"
#define BLYNK_TEMPLATE_NAME      "Quickstart Template"
#define BLYNK_AUTH_TOKEN         "vkcSMP8aSyww3QpoFJ-6pGkxVHxCmFBC"

#include <ESP8266WiFi.h>
#include <espnow.h>
#include <BlynkSimpleEsp8266.h>


// --- CẤU TRÚC DỮ LIỆU NHẬN (PHẢI GIỐNG HỆT ESP1) ---
// Cấu trúc này phải khớp 100% với struct_message bên Sender
typedef struct struct_message {
  int mq2_val;          // Mức MQ2
  int water1_val;       // Mức nước sensor 1
  int water2_val;       // Mức nước sensor 2
  int led_stt;          // Trạng thái LED chuyển động
  float temp_val;       // Nhiệt độ
  float hum_val;        // Độ ẩm
  int servo_ang;        // Góc servo hiện tại
} struct_message;

struct_message incomingData;

// --- HÀM CALLBACK KHI NHẬN DỮ LIỆU (ESP8266) ---
// Lưu ý: Signature của hàm này trên ESP8266 khác với ESP32 (không có const)
void OnDataRecv(uint8_t * mac, uint8_t *incomingBytes, uint8_t len) {
  // 1. Kiểm tra độ dài dữ liệu xem có khớp struct không
  if (len != sizeof(incomingData)) {
    Serial.println("Lỗi: Kích thước dữ liệu nhận được không khớp!");
    return;
  }

  // 2. Sao chép dữ liệu nhận được vào biến cấu trúc
  memcpy(&incomingData, incomingBytes, sizeof(incomingData));
  
  // 3. In ra Serial Monitor để kiểm tra
  Serial.println("------------------------------------------------");
  Serial.print("Nhan du lieu tu MAC: ");
  for(int i = 0; i < 6; i++){
    Serial.printf("%02X", mac[i]);
    if(i < 5) Serial.print(":");
  }
  Serial.println();
  
  // 4. Hiển thị chi tiết các thông số
  Serial.print("MQ2 (Gas/Khoi): "); 
  Serial.println(incomingData.mq2_val);
  
  Serial.print("Muc nuoc 1: "); 
  Serial.println(incomingData.water1_val);
  
  Serial.print("Muc nuoc 2: "); 
  Serial.println(incomingData.water2_val);
  
  Serial.print("Trang thai LED (Chuyen dong): "); 
  Serial.println(incomingData.led_stt == 1 ? "BAT" : "TAT");
  
  Serial.print("Nhiet do: "); 
  Serial.print(incomingData.temp_val);
  Serial.println(" *C");
  
  Serial.print("Do am: "); 
  Serial.print(incomingData.hum_val);
  Serial.println(" %");
  
  Serial.print("Goc Servo: "); 
  Serial.println(incomingData.servo_ang);
  Serial.println("------------------------------------------------");

  Blynk.virtualWrite(V2, incomingData.mq2_val);
  Blynk.virtualWrite(V4, incomingData.water1_val);
  Blynk.virtualWrite(V8, incomingData.water2_val);
  Blynk.virtualWrite(V3, incomingData.led_stt);
  Blynk.virtualWrite(V0, incomingData.temp_val);
  Blynk.virtualWrite(V1, incomingData.hum_val);
  Blynk.virtualWrite(V5, incomingData.servo_ang);
}

void setup() {
  Serial.begin(115200);

  // 1. Khởi động chế độ Station (Bắt buộc cho ESP-NOW)
  WiFi.mode(WIFI_STA);

  Serial.print("Receiver channel: ");
  Serial.println(WiFi.channel());   // để kiểm tra

  // 3) Khởi tạo ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Lỗi khởi tạo ESP-NOW");
    return;
  }
  
  // 4) Thiết lập vai trò là SLAVE (Chỉ nhận) - Bắt buộc với ESP8266
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);

  // 5) Đăng ký hàm nhận dữ liệu
  esp_now_register_recv_cb(OnDataRecv);

  Serial.println("ESP8266 Receiver dang lang nghe du lieu qua ESP-NOW...");
  
  // In địa chỉ MAC của ESP8266 để bạn điền vào code ESP1
  Serial.print("Dia chi MAC cua ESP8266 (Receiver): ");
  Serial.println(WiFi.macAddress());

  Blynk.begin(BLYNK_AUTH_TOKEN,"Duong192004","88888888");
  Serial.println("Đã kết nối Blynk!");
}

void loop() {
  Blynk.run();
  delay(1000); 
}