#include <Arduino.h>
#include <ESP32Servo.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>

// --- THƯ VIỆN ESP-NOW VÀ WIFI ---
#include <esp_now.h>
#include <WiFi.h>

// --- CẤU HÌNH ĐỊA CHỈ MAC NGƯỜI NHẬN (QUAN TRỌNG) ---
// Thay thế các số 0xFF bằng địa chỉ MAC của ESP8266 bạn vừa lấy ở Bước 1
// Ví dụ: {0x24, 0x6F, 0x28, 0x10, 0x5C, 0xA4}
uint8_t broadcastAddress[] = {0xBC, 0xDD, 0xC2, 0x7A, 0x0D, 0x19};

// --- CẤU TRÚC DỮ LIỆU ---
typedef struct struct_message
{
    int mq2_val;
    int water1_val;
    int water2_val;
    int led_stt;
    float temp_val;
    float hum_val;
    int servo_ang;
} struct_message;

struct_message myData;

// Khai báo thông tin Peer (đối tác nhận)
esp_now_peer_info_t peerInfo;

// --- BIẾN TOÀN CỤC ---
float global_temp = 0.0;
float global_hum = 0.0;
unsigned long previousMillisSend = 0;
const long intervalSend = 3000;

// --- CẤU HÌNH DHT ---
#define DHTTYPE DHT11

const int MQ2_THRESHOLD = 3000;

// Chân thiết bị
int water_sensor_1 = 34;
int water_sensor_2 = 35;
int move_sensor = 4;
int led_pin = 12;
int servo1 = 5;
int up_servo = 19;
int down_servo = 25;
int reset_button = 13;
int turn_off_system = 32;
int led_inside_pin = 33;
int buzzer_pin = 26;
int servo2 = 21; // Lưu ý: Biến này khai báo nhưng chưa thấy dùng attach
int dht_pin = 27;
int mq2 = 35; // Lưu ý: Bạn đang dùng chân 35 cho cả water_sensor_2 và mq2? Hãy kiểm tra lại phần cứng. ESP32 chân 35 chỉ là Input.

// Đối tượng
Servo myServo;
int servo_hand = 1;
int servo_angle = 0;
int flood_detected = 0;
int trang_thai = 1;

bool blynk_led_override = false;
bool blynk_servo_override = false;

// Timer
unsigned long previousMillisDHT = 0;
const long intervalDHT = 2000;

// Timer LOG
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
void sendDataESPNow(); // Hàm gửi mới dùng ESP-NOW

// Hàm callback khi gửi xong (để biết gửi thành công hay thất bại)
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    // Serial.print("\r\nTrang thai gui goi tin: ");
    // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Thanh cong" : "That bai");
}

void setup()
{
    Serial.begin(115200);

    myServo.attach(servo1);
    pinMode(water_sensor_1, INPUT);
    // pinMode(water_sensor_2, INPUT); // Chân 35
    pinMode(move_sensor, INPUT);
    pinMode(led_pin, OUTPUT);
    pinMode(up_servo, INPUT_PULLDOWN);
    pinMode(down_servo, INPUT_PULLDOWN);
    pinMode(reset_button, INPUT_PULLDOWN);
    pinMode(turn_off_system, INPUT_PULLDOWN);
    pinMode(led_inside_pin, OUTPUT);
    pinMode(buzzer_pin, OUTPUT);
    digitalWrite(buzzer_pin, LOW);

    myServo.write(servo_angle);
    dht.begin();

    // --- KẾT NỐI ESP-NOW ---
    // ESP-NOW dùng chế độ Station
    WiFi.mode(WIFI_STA);
    WiFi.begin("Duong192004", "88888888");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(100);
    }
    Serial.print("ESP32 channel: ");
    Serial.println(WiFi.channel());
    // Khởi động ESP-NOW
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Loi khoi tao ESP-NOW");
        return;
    }

    // Đăng ký hàm callback khi gửi
    esp_now_register_send_cb(OnDataSent);

    // Đăng ký Peer (Người nhận)
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        Serial.println("Loi ket noi voi nguoi nhan (Add Peer failed)");
        return;
    }

    Serial.println("He thong san sang (Mode: ESP-NOW)");
}

// --- HÀM GỬI DỮ LIỆU QUA ESP-NOW ---
void sendDataESPNow()
{
    // 1. Cập nhật dữ liệu vào struct
    myData.mq2_val = analogRead(mq2);
    myData.water1_val = analogRead(water_sensor_1);
    myData.water2_val = analogRead(water_sensor_2);
    myData.led_stt = digitalRead(led_pin);
    myData.servo_ang = servo_angle;
    myData.temp_val = global_temp;
    myData.hum_val = global_hum;

    // 2. Gửi gói tin
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&myData, sizeof(myData));

    if (result == ESP_OK)
    {
        // Gửi thành công (có thể in log nếu muốn)
    }
    else
    {
        Serial.println("Loi gui tin ESP-NOW");
    }
}

void readDHTAndDisplay()
{
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t))
    {
        Serial.println(F("Loi khi doc tu cam bien DHT!"));
        return;
    }

    global_temp = t;
    global_hum = h;

    Serial.print(F("Do Am: "));
    Serial.print(h);
    Serial.print(F(" %\t"));
    Serial.print(F("Nhiet Do: "));
    Serial.print(t);
    Serial.println(F(" *C"));
}

void readMQ2AndAlarm()
{
    int sensorValue = analogRead(mq2);
    unsigned long currentMillis = millis();

    if (sensorValue > MQ2_THRESHOLD)
    {
        digitalWrite(buzzer_pin, HIGH);
    }
    else
    {
        digitalWrite(buzzer_pin, LOW);
    }

    if (currentMillis - previousLogMQ2 >= logInterval)
    {
        previousLogMQ2 = currentMillis;
        Serial.print("MQ-2 Value: ");
        Serial.print(sensorValue);
        if (sensorValue > MQ2_THRESHOLD)
        {
            Serial.print(" -> KHÍ PHÁT HIỆN!");
        }
        Serial.println();
    }
}

void changeServoSmoothly(int fromAngle, int toAngle)
{
    int step = 5;
    if (fromAngle < toAngle)
    {
        for (int angle = fromAngle; angle <= toAngle; angle += step)
        {
            if (angle > toAngle)
                angle = toAngle;
            myServo.write(angle);
            delay(150);
        }
    }
    else
    {
        for (int angle = fromAngle; angle >= toAngle; angle -= step)
        {
            if (angle < toAngle)
                angle = toAngle;
            myServo.write(angle);
            delay(150);
        }
    }
    servo_angle = toAngle;
}

void movement_detected()
{
    if (blynk_led_override)
        return;

    unsigned long currentMillis = millis();

    if (digitalRead(move_sensor) == HIGH)
    {
        digitalWrite(led_pin, HIGH);

        if (currentMillis - previousLogMotion >= logInterval)
        {
            previousLogMotion = currentMillis;
            Serial.println("Phát hiện chuyển động!");
        }
    }
    else
    {
        digitalWrite(led_pin, LOW);
    }
}

void reset_system()
{
    servo_hand = 1;
    flood_detected = 0;
    Serial.println("he thong reset");
}

void ha_chong_lu()
{
    if (blynk_servo_override)
        return;

    Serial.println("THỦ CÔNG - Hạ chống lũ");
    while (digitalRead(down_servo) == HIGH && servo_angle > 0)
    {
        changeServoSmoothly(servo_angle, servo_angle - 5);
    }
}

void nang_chong_lu()
{
    if (blynk_servo_override)
        return;

    Serial.println("THỦ CÔNG - Nâng chống lũ");
    while (digitalRead(up_servo) == HIGH && servo_angle < 100)
    {
        changeServoSmoothly(servo_angle, servo_angle + 5);
    }
}

void chong_lu()
{
    if (blynk_servo_override)
        return;

    if (servo_hand == 1)
    {
        int water_level = analogRead(water_sensor_1);

        if (water_level > 3000 && flood_detected == 0)
        {
            Serial.println("CẢNH BÁO: PHÁT HIỆN LŨ - Nâng chống lũ tự động");
            changeServoSmoothly(servo_angle, 100);
            flood_detected = 1;
        }
    }
}

void toggle_status(int &temp)
{
    if (temp == 0)
        temp = 1;
    else
        temp = 0;
}

void system_change()
{
    if (digitalRead(turn_off_system) == HIGH)
    {
        toggle_status(trang_thai);
        delay(300);
    }
}

void tong_hop()
{
    if (digitalRead(reset_button) == HIGH)
    {
        reset_system();
        delay(500);
    }

    movement_detected();
    readMQ2AndAlarm();

    if (digitalRead(up_servo) == HIGH)
    {
        servo_hand = 0;
        nang_chong_lu();
    }
    else if (digitalRead(down_servo) == HIGH)
    {
        servo_hand = 0;
        ha_chong_lu();
    }
    else
    {
        chong_lu();
    }

    // Timer đọc DHT (2 giây/lần)
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillisDHT >= intervalDHT)
    {
        previousMillisDHT = currentMillis;
        readDHTAndDisplay();
    }

    // Timer gửi dữ liệu ESP-NOW (1 giây/lần)
    if (currentMillis - previousMillisSend >= intervalSend)
    {
        previousMillisSend = currentMillis;
        sendDataESPNow(); // <--- GỌI HÀM GỬI MỚI
    }
}

void loop()
{
    system_change();
    if (trang_thai == 1)
    {
        tong_hop();
    }
    else
    {
        digitalWrite(buzzer_pin, LOW);
        Serial.println("HỆ THỐNG ĐANG TẮT");
        delay(2000);
    }
}