#include <Arduino.h>
#include <ESP32Servo.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <LiquidCrystal_I2C.h>

const char *ssid = "ESP32_GATEWAY";
const char *password = "12345678";
const char *udpAddress = "192.168.4.1";

const int udpPort = 4210;
WiFiUDP udp;

LiquidCrystal_I2C lcd(0x27, 16, 2);

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

typedef struct struct_command
{
  int command; 
} struct_command;
struct_command cmdData;

float global_temp = 0.0;
float global_hum = 0.0;
unsigned long previousMillisSend = 0;
const long intervalSend = 300;

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
int servo_angle = 15;
int flood_detected = 0;
int trang_thai = 1;

bool led_manual_mode = false;

unsigned long previousMillisDHT = 0;
const long intervalDHT = 2000;

DHT dht(dht_pin, DHTTYPE);

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

void setup()
{
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();
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

  lcd.setCursor(0, 0); 
  lcd.print("Wifi connecting");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nDa ket noi Wifi Gateway!");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Wifi Connected!");
  delay(1000);
  lcd.clear(); 

  udp.begin(udpPort);
}

void sendDataUDP()
{
  if (WiFi.status() != WL_CONNECTED)
    return;
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

void receiveCommandUDP()
{
  int packetSize = udp.parsePacket();
  if (packetSize)
  {
    int len = udp.read((char *)&cmdData, sizeof(cmdData));
    if (len > 0)
    {
      if (cmdData.command == 1)
      {
        servo_hand = 0;
        nang_chong_lu();
      }
      else if (cmdData.command == 2)
      {
        servo_hand = 0;
        ha_chong_lu();
      }
      else if (cmdData.command == 3)
      {
        reset_system();
      }
      else if (cmdData.command == 5)
      {
        led_manual_mode = true;
        digitalWrite(led_pin, HIGH);
        Serial.println("LENH: Bat den (Manual Mode)");
      }
    }
  }
}

void readDHTAndDisplay()
{
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t))
    return;
  global_temp = t;
  global_hum = h;

  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(t, 1);
  lcd.print(" C  ");
  lcd.setCursor(0, 1);
  lcd.print("Hum : ");
  lcd.print(h, 1);
  lcd.print(" %  ");
}

void readMQ2AndAlarm()
{
  int val = analogRead(mq2);
  if (val > MQ2_THRESHOLD)
    digitalWrite(buzzer_pin, HIGH);
  else
    digitalWrite(buzzer_pin, LOW);
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
      delay(40); 
    }
  }
  else
  {
    for (int angle = fromAngle; angle >= toAngle; angle -= step)
    {
      if (angle < toAngle)
        angle = toAngle;
      myServo.write(angle);
      delay(40); 
    }
  }
  servo_angle = toAngle;
}

void movement_detected()
{
  if (led_manual_mode == true)
  {
    digitalWrite(led_pin, HIGH);
  }
  else
  {
    if (digitalRead(move_sensor) == HIGH)
      digitalWrite(led_pin, HIGH);
    else
      digitalWrite(led_pin, LOW);
  }
}

void reset_system()
{
  servo_hand = 1;
  flood_detected = 0;
  led_manual_mode = false;

  
  lcd.clear();

  Serial.println("HE THONG DA RESET (Remote)");
}

void ha_chong_lu()
{
  Serial.println("LENH: Ha chong lu");
  while ((digitalRead(down_servo) == HIGH || cmdData.command == 2) && servo_angle > 14)
  {
    changeServoSmoothly(servo_angle, servo_angle - 5);
  }
  cmdData.command = 0;
}

void nang_chong_lu()
{
  Serial.println("LENH: Nang chong lu");
  while ((digitalRead(up_servo) == HIGH || cmdData.command == 1) && servo_angle < 150)
  {
    changeServoSmoothly(servo_angle, servo_angle + 5);
  }
  cmdData.command = 0;
}

void chong_lu()
{
  if (servo_hand == 1)
  {
    int water_level = analogRead(water_sensor_1);
    if (water_level > 3000 && flood_detected == 0)
    {
      changeServoSmoothly(servo_angle, 150);
      flood_detected = 1;
    }
  }
}

void toggle_status(int &temp) { 
  if(temp == 1) 
    temp = 0; 
  else 
    temp = 1;
    lcd.clear();
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

  receiveCommandUDP();

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

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillisDHT >= intervalDHT)
  {
    previousMillisDHT = currentMillis;
    readDHTAndDisplay();
  }
  if (currentMillis - previousMillisSend >= intervalSend)
  {
    previousMillisSend = currentMillis;
    sendDataUDP();
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
    digitalWrite(led_pin, LOW);
    Serial.println("SYSTEM OFF");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SYSTEM OFF");
    delay(200);
  }
}
