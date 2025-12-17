#include <Arduino.h>
#include <ESP32Servo.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <LiquidCrystal_I2C.h>

// --- WIFI CONFIGURATION ---
const char *ssid = "ESP32_GATEWAY";
const char *password = "12345678";
const char *udpAddress = "192.168.4.1"; 
const int udpPort = 4210;
WiFiUDP udp;

// --- LCD ---
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// --- DATA STRUCTURES ---
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
    int command; 
} struct_command;
struct_command cmdData;

// --- GLOBAL VARIABLES & PINS ---
float global_temp = 0.0;
float global_hum = 0.0;
unsigned long previousMillisSend = 0;
const long intervalSend = 1000; 
#define DHTTYPE DHT11
const int MQ2_THRESHOLD = 3000;
const int WATER_THRESHOLD = 3000;

const int water_sensor_1 = 34;
const int move_sensor = 4;
const int led_pin = 12;
const int servo_pin = 5;
const int btn_up = 19;
const int btn_down = 25;
const int btn_reset = 13;
const int btn_power = 32;
const int buzzer_pin = 26;
const int dht_pin = 27;
const int mq2_pin = 35; 

Servo myServo;
int servo_mode = 1; 
int servo_angle = 0;
int flood_flag = 0;
int system_status = 1; 
unsigned long previousMillisDHT = 0;
const long intervalDHT = 2000;
DHT dht(dht_pin, DHTTYPE);

// --- FUNCTIONS ---
void moveServoSmoothly(int fromAngle, int toAngle);
void updateSensors(); 
void checkGasAndAlarm();
void handleMotionLight();
void resetSystem();
void closeGate();
void openGate();
void autoFloodControl();
void sendDataUDP();
void receiveCommandUDP(); 

void setup() {
    Serial.begin(115200);
    lcd.init(); lcd.backlight();
    lcd.print("System Starting");
    myServo.attach(servo_pin);
    pinMode(water_sensor_1, INPUT);
    pinMode(move_sensor, INPUT);
    pinMode(led_pin, OUTPUT);
    pinMode(btn_up, INPUT_PULLDOWN);
    pinMode(btn_down, INPUT_PULLDOWN);
    pinMode(btn_reset, INPUT_PULLDOWN);
    pinMode(btn_power, INPUT_PULLDOWN);
    pinMode(buzzer_pin, OUTPUT);
    digitalWrite(buzzer_pin, LOW);
    myServo.write(servo_angle);
    dht.begin();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    lcd.clear(); lcd.print("WiFi Connected!");
    udp.begin(udpPort); 
}

void sendDataUDP() {
    if(WiFi.status() != WL_CONNECTED) return;
    myData.mq2_val = analogRead(mq2_pin);
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
            switch(cmdData.command) {
                case 1: servo_mode = 0; openGate(); break;
                case 2: servo_mode = 0; closeGate(); break;
                case 3: resetSystem(); break;
                case 5: // Light Toggle
                    digitalWrite(led_pin, !digitalRead(led_pin)); 
                    break; // THÊM BREAK Ở ĐÂY
                case 6: // Buzzer
                    digitalWrite(buzzer_pin, HIGH); delay(500);
                    digitalWrite(buzzer_pin, LOW); break;
            }
        }
    }
}

void updateSensors() {
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (isnan(h) || isnan(t)) return;
    global_temp = t; global_hum = h;
    lcd.setCursor(0, 0); lcd.printf("T: %.1fC  H: %.0f%%", t, h);
    lcd.setCursor(0, 1); lcd.printf("Mode: %s", (servo_mode==1)?"AUTO":"MAN ");
}

void checkGasAndAlarm() {
    if (analogRead(mq2_pin) > MQ2_THRESHOLD) digitalWrite(buzzer_pin, HIGH);
    else digitalWrite(buzzer_pin, LOW);
}

void moveServoSmoothly(int fromAngle, int toAngle) {
    int step = (fromAngle < toAngle) ? 5 : -5;
    int angle = fromAngle;
    while (angle != toAngle) {
        angle += step;
        if ((step > 0 && angle > toAngle) || (step < 0 && angle < toAngle)) angle = toAngle;
        myServo.write(angle);
        delay(30); 
    }
    servo_angle = toAngle;
}

void handleMotionLight() {
    digitalWrite(led_pin, digitalRead(move_sensor));
}

void resetSystem() {
    servo_mode = 1; flood_flag = 0;
    moveServoSmoothly(servo_angle, 0);
}

void closeGate() { moveServoSmoothly(servo_angle, 0); }
void openGate() { moveServoSmoothly(servo_angle, 100); }

void autoFloodControl() {
    if (servo_mode == 1 && analogRead(water_sensor_1) > WATER_THRESHOLD && flood_flag == 0) {
        openGate();
        flood_flag = 1;
    }
}

void mainTasks() {
    if (digitalRead(btn_reset) == HIGH) { resetSystem(); delay(500); }
    receiveCommandUDP(); 
    handleMotionLight();
    checkGasAndAlarm();

    if (digitalRead(btn_up) == HIGH) { servo_mode = 0; openGate(); }
    else if (digitalRead(btn_down) == HIGH) { servo_mode = 0; closeGate(); }
    else { autoFloodControl(); }

    unsigned long currentMillis = millis();
    if (currentMillis - previousMillisDHT >= intervalDHT) {
        previousMillisDHT = currentMillis;
        updateSensors(); 
    }
    if (currentMillis - previousMillisSend >= intervalSend) {
        previousMillisSend = currentMillis;
        sendDataUDP();
    }
}

void loop() {
    if (digitalRead(btn_power) == HIGH) { system_status = !system_status; delay(300); }

    if (system_status == 1) mainTasks();
    else {
        digitalWrite(buzzer_pin, LOW); digitalWrite(led_pin, LOW);
        lcd.clear(); lcd.print("SYSTEM OFF"); delay(500);
    }
}