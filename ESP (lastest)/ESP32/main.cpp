#include <Arduino.h>
#include <ESP32Servo.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <LiquidCrystal_I2C.h>

// --- CẤU HÌNH WIFI ---
const char *ssid = "ESP32_GATEWAY";
const char *password = "12345678";
const char *udpAddress = "192.168.4.1"; 
const int udpPort = 4210;

WiFiUDP udp;

// --- CẤU HÌNH LCD ---
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// --- CẤU TRÚC GÓI TIN ---
typedef struct struct_message {
    int mq2_val;
    int water1_val;
    int water2_val;
    int led_stt;
    float temp_val;
    float hum_val;
    int servo_ang;
} struct_message;
struct_message myData;

typedef struct struct_command {
    int command; // 1: Nâng, 2: Hạ, 3: Reset
} struct_command;
struct_command cmdData;

// --- BIẾN TOÀN CỤC ---
float global_temp = 0.0;
float global_hum = 0.0;
unsigned long previousMillisSend = 0;
const long intervalSend = 1000; 

#define DHTTYPE DHT11
const int MQ2_THRESHOLD = 3000;

int water_sensor_1 = 34;
int move_sensor = 4;
int led_pin = 12;
int servo1 = 5;
int up_servo = 19;
int down_servo = 25;
int reset_button = 13;
int turn_off_system = 32;
int buzzer_pin = 26;
int dht_pin = 27;
int mq2 = 35; 

Servo myServo;
int servo_hand = 1; 
int servo_angle = 0;
int flood_detected = 0;
int trang_thai = 1;

bool blynk_led_override = false;
bool blynk_servo_override = false;

unsigned long previousMillisDHT = 0;
const long intervalDHT = 2000;
unsigned long previousLogMQ2 = 0;
unsigned long previousLogMotion = 0;
const long logInterval = 1000;

DHT dht(dht_pin, DHTTYPE);

// --- KHAI BÁO HÀM ---
void changeServoSmoothly(int fromAngle, int toAngle);
void readDHTAndDisplay();
void readMQ2AndAlarm();
void movement_detected();
void reset_system();
void ha_chong_lu();
void nang_chong_lu();
void chong_lu();
void toggle_status(int &temp);
void system_change();
void tong_hop();
void sendDataUDP();
void receiveCommandUDP(); 

void setup() {
    Serial.begin(115200);
    
    lcd.init();      
    lcd.backlight(); 
    lcd.setCursor(0, 0);
    lcd.print("System Starting");
    
    myServo.attach(servo1);
    pinMode(water_sensor_1, INPUT);
    pinMode(move_sensor, INPUT);
    pinMode(led_pin, OUTPUT);
    pinMode(up_servo, INPUT_PULLDOWN);
    pinMode(down_servo, INPUT_PULLDOWN);
    pinMode(reset_button, INPUT_PULLDOWN);
    pinMode(turn_off_system, INPUT_PULLDOWN);
    pinMode(buzzer_pin, OUTPUT);
    digitalWrite(buzzer_pin, LOW);
    myServo.write(servo_angle);
    dht.begin();

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Dang ket noi Gateway: ");
    
    lcd.setCursor(0, 1);
    lcd.print("Wifi connecting");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500); Serial.print(".");
    }
    Serial.println("\nDa ket noi Wifi Gateway!");
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Wifi Connected!");
    delay(1000);
    lcd.clear();

    udp.begin(udpPort); 
}

void sendDataUDP() {
    if(WiFi.status() != WL_CONNECTED) return;
    myData.mq2_val = analogRead(mq2);
    myData.water1_val = analogRead(water_sensor_1);
    myData.led_stt = digitalRead(led_pin);
    myData.servo_ang = servo_angle;
    myData.temp_val = global_temp;
    myData.hum_val = global_hum;

    udp.beginPacket(udpAddress, udpPort);
    udp.write((const uint8_t *)&myData, sizeof(myData));
    udp.endPacket();
}

void receiveCommandUDP() {
    int packetSize = udp.parsePacket();
    if (packetSize) {
        int len = udp.read((char*)&cmdData, sizeof(cmdData));
        if (len > 0) {
            Serial.print("Nhan lenh tu Gateway: ");
            Serial.println(cmdData.command);

            if (cmdData.command == 1) { 
                servo_hand = 0; 
                nang_chong_lu();
            } 
            else if (cmdData.command == 2) { 
                servo_hand = 0; 
                ha_chong_lu();
            } 
            else if (cmdData.command == 3) { 
                reset_system();
            }
        }
    }
}

void readDHTAndDisplay() {
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (isnan(h) || isnan(t)) return;
    global_temp = t; global_hum = h;

    lcd.setCursor(0, 0); 
    lcd.print("Temp: "); lcd.print(t, 1); lcd.print(" C");
    lcd.setCursor(0, 1); 
    lcd.print("Hum : "); lcd.print(h, 1); lcd.print(" %");
}

void readMQ2AndAlarm() {
    int val = analogRead(mq2);
    if (val > MQ2_THRESHOLD) digitalWrite(buzzer_pin, HIGH);
    else digitalWrite(buzzer_pin, LOW);
}

void changeServoSmoothly(int fromAngle, int toAngle) {
    int step = 5;
    if (fromAngle < toAngle) {
        for (int angle = fromAngle; angle <= toAngle; angle += step) {
            if (angle > toAngle) angle = toAngle;
            myServo.write(angle); delay(100);
        }
    } else {
        for (int angle = fromAngle; angle >= toAngle; angle -= step) {
            if (angle < toAngle) angle = toAngle;
            myServo.write(angle); delay(100);
        }
    }
    servo_angle = toAngle;
}

void movement_detected() {
    if (digitalRead(move_sensor) == HIGH) digitalWrite(led_pin, HIGH);
    else digitalWrite(led_pin, LOW);
}

void reset_system() {
    servo_hand = 1;
    flood_detected = 0;
    Serial.println("HE THONG DA RESET (Remote)");
}

// --- ĐÃ SỬA: Chạy 1 mạch đến khi hết hành trình ---
void ha_chong_lu() {
    Serial.println("LENH: Ha chong lu");
    
    // Vòng lặp sẽ chạy liên tục nếu Nút bấm giữ HOẶC có lệnh Command = 2
    while ((digitalRead(down_servo) == HIGH || cmdData.command == 2) && servo_angle > 0) {
        changeServoSmoothly(servo_angle, servo_angle - 5);
        // Đã xóa dòng break để nó không dừng lại giữa chừng
    }
    cmdData.command = 0; // Xóa lệnh sau khi đã chạy xong
}

// --- ĐÃ SỬA: Chạy 1 mạch đến khi hết hành trình ---
void nang_chong_lu() {
    Serial.println("LENH: Nang chong lu");
    
    // Vòng lặp sẽ chạy liên tục nếu Nút bấm giữ HOẶC có lệnh Command = 1
    while ((digitalRead(up_servo) == HIGH || cmdData.command == 1) && servo_angle < 100) {
        changeServoSmoothly(servo_angle, servo_angle + 5);
        // Đã xóa dòng break để nó không dừng lại giữa chừng
    }
    cmdData.command = 0; // Xóa lệnh sau khi đã chạy xong
}

void chong_lu() {
    if (servo_hand == 1) {
        int water_level = analogRead(water_sensor_1);
        if (water_level > 3000 && flood_detected == 0) {
            changeServoSmoothly(servo_angle, 100);
            flood_detected = 1;
        }
    }
}

void toggle_status(int &temp) { temp = (temp == 0) ? 1 : 0; }
void system_change() { 
    if (digitalRead(turn_off_system) == HIGH) { 
        toggle_status(trang_thai); delay(300); 
    } 
}

void tong_hop() {
    if (digitalRead(reset_button) == HIGH) { reset_system(); delay(500); }
    
    receiveCommandUDP(); 

    movement_detected();
    readMQ2AndAlarm();

    if (digitalRead(up_servo) == HIGH) { servo_hand = 0; nang_chong_lu(); }
    else if (digitalRead(down_servo) == HIGH) { servo_hand = 0; ha_chong_lu(); }
    else { chong_lu(); }

    unsigned long currentMillis = millis();
    if (currentMillis - previousMillisDHT >= intervalDHT) {
        previousMillisDHT = currentMillis;
        readDHTAndDisplay();
    }
    if (currentMillis - previousMillisSend >= intervalSend) {
        previousMillisSend = currentMillis;
        sendDataUDP();
    }
}

void loop() {
    system_change();
    if (trang_thai == 1) tong_hop();
    else {
        digitalWrite(buzzer_pin, LOW);
        Serial.println("HỆ THỐNG ĐANG TẮT");
        lcd.clear(); 
        lcd.setCursor(0,0);
        lcd.print("SYSTEM OFF");
        delay(200);
    }
}

// #include <WiFi.h>
// #include <WiFiUdp.h>
// #include <Firebase_ESP_Client.h>

// // Cung cấp các hàm hỗ trợ tạo Token và tính toán RTDB (Bắt buộc cho thư viện Mobizt)
// #include <addons/TokenHelper.h>
// #include <addons/RTDBHelper.h>

// // --- 1. CẤU HÌNH WIFI ---
// char ssid_internet[] = "Duong192004";
// char pass_internet[] = "88888888";

// const char *ssid_ap = "ESP32_GATEWAY";
// const char *pass_ap = "12345678";

// // --- 2. CẤU HÌNH FIREBASE ---
// // Lưu ý: Host không có "https://" và không có "/" ở cuối
// #define FIREBASE_HOST "iotprj-537a3-default-rtdb.asia-southeast1.firebasedatabase.app"
// #define FIREBASE_AUTH "F824sV1vzGRd5eUfRs7qqAPRckRbGunYuFkGvn6w"

// // --- ĐỐI TƯỢNG FIREBASE ---
// FirebaseData fbdo;
// FirebaseAuth auth;
// FirebaseConfig config;

// // --- CẤU HÌNH UDP ---
// WiFiUDP udp;
// const int udpPort = 4210;
// IPAddress sensorIP;
// bool sensorConnected = false;

// // --- STRUCT DỮ LIỆU (Giống hệt con Sensor) ---
// typedef struct struct_message {
//     int mq2_val;
//     int water1_val;
//     int water2_val;
//     int led_stt;
//     float temp_val;
//     float hum_val;
//     int servo_ang;
// } struct_message;

// typedef struct struct_command {
//     int command; // 1: Up, 2: Down, 3: Reset
// } struct_command;

// struct_command cmdData;
// struct_message myData;

// // Biến timer
// unsigned long lastFirebaseCheck = 0;
// bool taskCompleted = false;

// void setup() {
//     Serial.begin(115200);

//     // 1. Kết nối Wifi Internet & Phát Wifi AP
//     WiFi.mode(WIFI_AP_STA);
//     WiFi.softAP(ssid_ap, pass_ap);
//     WiFi.begin(ssid_internet, pass_internet);

//     Serial.print("Dang ket noi Internet");
//     while (WiFi.status() != WL_CONNECTED) {
//         Serial.print(".");
//         delay(500);
//     }
//     Serial.println("\nDa ket noi Internet!");
//     Serial.print("IP Wan: "); Serial.println(WiFi.localIP());
//     Serial.print("IP Lan: "); Serial.println(WiFi.softAPIP());

//     // 2. Khởi tạo Firebase
//     config.api_key = ""; // Nếu dùng Auth Token thì để trống API Key
//     config.database_url = FIREBASE_HOST;
//     config.signer.tokens.legacy_token = FIREBASE_AUTH;

//     // Tự động kết nối lại khi mất mạng
//     Firebase.reconnectWiFi(true);
    
//     // Tăng kích thước buffer nhận dữ liệu (cần thiết cho ESP32)
//     fbdo.setResponseSize(4096);

//     Firebase.begin(&config, &auth);
    
//     // Chờ kết nối Firebase
//     while (!Firebase.ready()) {
//         Serial.print(".");
//         delay(100);
//     }
//     Serial.println("\nFirebase Ready!");

//     // 3. Khởi tạo UDP
//     udp.begin(udpPort);
//     Serial.println("UDP Server Started");
// }

// // --- HÀM GỬI LỆNH UDP VỀ SENSOR ---
// void sendCommandToSensor(int cmd) {
//     if (sensorConnected) {
//         cmdData.command = cmd;
//         udp.beginPacket(sensorIP, udpPort);
//         udp.write((const uint8_t*)&cmdData, sizeof(cmdData));
//         udp.endPacket();
//         Serial.print("-> Da gui lenh UDP: "); Serial.println(cmd);
//     } else {
//         Serial.println("Loi: Chua co Sensor nao ket noi den Gateway!");
//     }
// }

// // --- HÀM ĐẨY DỮ LIỆU LÊN FIREBASE ---
// void pushDataToFirebase() {
//     if (Firebase.ready()) {
//         // Sử dụng FirebaseJson để đóng gói dữ liệu gọn gàng
//         FirebaseJson json;
        
//         // Thêm dữ liệu vào JSON
//         json.set("temp", myData.temp_val);
//         json.set("hum", myData.hum_val);
//         json.set("gas_val", myData.mq2_val);
//         json.set("water1", myData.water1_val);
//         json.set("water2", myData.water2_val);
//         json.set("led_motion", myData.led_stt);
//         json.set("servo_angle", myData.servo_ang);

//         // Tạo trạng thái cảnh báo text
//         String statusText = "Normal";
//         if (myData.water1_val > 3000) statusText = "LŨ KHẨN CẤP!";
//         else if (myData.mq2_val > 3000) statusText = "RÒ RỈ GAS!";
//         json.set("status", statusText);

//         // Đẩy toàn bộ cục JSON lên nhánh "/monitor"
//         // updateNode sẽ không xóa các dữ liệu khác, chỉ cập nhật cái mới
//         if (Firebase.RTDB.updateNode(&fbdo, "/monitor", &json)) {
//             Serial.println("Da day du lieu len Firebase: OK");
//         } else {
//             Serial.print("Loi Firebase: ");
//             Serial.println(fbdo.errorReason());
//         }
        
//         // Xử lý Alert riêng (Ví dụ để kích hoạt App khác)
//         if (myData.water1_val > 3000) Firebase.RTDB.setInt(&fbdo, "/alerts/flood", 1);
//         else Firebase.RTDB.setInt(&fbdo, "/alerts/flood", 0);
//     }
// }

// // --- HÀM KIỂM TRA LỆNH TỪ FIREBASE ---
// void checkFirebaseCommand() {
//     if (Firebase.ready()) {
//         // Đọc giá trị từ đường dẫn "/control/cmd"
//         // 0: Ko lam gi, 1: Nang, 2: Ha, 3: Reset
//         if (Firebase.RTDB.getInt(&fbdo, "/control/cmd")) {
//             if (fbdo.dataType() == "int") {
//                 int cmd = fbdo.intData();
                
//                 if (cmd != 0) {
//                     Serial.print("Nhan lenh Firebase: "); Serial.println(cmd);
                    
//                     // Gửi lệnh qua UDP
//                     sendCommandToSensor(cmd);
                    
//                     // Reset biến trên Firebase về 0 ngay lập tức
//                     // Để tạo hiệu ứng "nút nhấn nhả"
//                     Firebase.RTDB.setInt(&fbdo, "/control/cmd", 0);
//                 }
//             }
//         }
//     }
// }

// void loop() {
//     // 1. NHẬN UDP TỪ SENSOR
//     int packetSize = udp.parsePacket();
//     if (packetSize) {
//         sensorIP = udp.remoteIP();
//         sensorConnected = true;

//         if (packetSize == sizeof(myData)) {
//             udp.read((char*)&myData, sizeof(myData));

//             // Log ra Serial để kiểm tra
//             Serial.print("Nhan tu Sensor -> T: "); Serial.print(myData.temp_val);
//             Serial.print(" | H: "); Serial.print(myData.hum_val);
//             Serial.print(" | W: "); Serial.println(myData.water1_val);

//             // Đẩy lên Firebase
//             pushDataToFirebase();
//         } else {
//             // Xóa buffer nếu gói tin rác
//             char temp[255];
//             udp.read(temp, packetSize);
//         }
//     }

//     // 2. KIỂM TRA LỆNH TỪ FIREBASE (Mỗi 500ms check 1 lần)
//     if (millis() - lastFirebaseCheck > 500) {
//         lastFirebaseCheck = millis();
//         checkFirebaseCommand();
//     }
// }