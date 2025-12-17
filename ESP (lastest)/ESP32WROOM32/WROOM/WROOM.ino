#include <WiFi.h>
#include <WiFiUdp.h>
#include <Firebase_ESP_Client.h>
#include <ESP_Mail_Client.h>
#include <WebServer.h>
#include <ElegantOTA.h>

// Firebase interfaces
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// --- 1. CONFIGURATION ---
char ssid_internet[] = "Duong192004";
char pass_internet[] = "88888888";

const char *ssid_ap = "ESP32_GATEWAY";
const char *pass_ap = "12345678";

#define FIREBASE_HOST "iotprj-537a3-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "F824sV1vzGRd5eUfRs7qqAPRckRbGunYuFkGvn6w"

WebServer server(80);

// ======================= Email configurations =======================
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define AUTHOR_EMAIL "your_email@gmail.com"      
#define AUTHOR_PASSWORD "your_app_password" 
#define RECIPIENT_EMAIL "receiver_email@gmail.com" 

SMTPSession smtp;
void smtpCallback(SessionStatus status); 
unsigned long lastMailMillis = 0;
// =========================================================================

// --- OBJECTS ---
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

WiFiUDP udp;
const int udpPort = 4210;
IPAddress sensorIP;
bool sensorConnected = false;

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

typedef struct struct_command {
    int command; 
} struct_command;

struct_command cmdData;
struct_message myData;

unsigned long lastFirebaseCheck = 0;

// ======================= Send email function =======================
void sendWarningEmail(String subject, String message) {
    // Chống spam: Chỉ gửi 1 mail mỗi 5 phút
    if (lastMailMillis != 0 && millis() - lastMailMillis < 300000) return;

    ESP_Mail_Session session;
    session.server.host_name = SMTP_HOST;
    session.server.port = SMTP_PORT;
    session.login.email = AUTHOR_EMAIL;
    session.login.password = AUTHOR_PASSWORD;
    session.login.user_domain = "";

    SMTP_Message msg;
    msg.sender.name = "HomeX Security System";
    msg.sender.email = AUTHOR_EMAIL;
    msg.subject = subject;
    msg.addRecipient("User", RECIPIENT_EMAIL);
    msg.text.content = message.c_str();

    if (!smtp.connect(&session)) return;
    if (!MailClient.sendMail(&smtp, &msg)) {
        Serial.println("Error sending Email, " + smtp.errorReason());
    }
    lastMailMillis = millis();
}
// =========================================================================

void setup() {
    Serial.begin(115200);

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(ssid_ap, pass_ap);
    WiFi.begin(ssid_internet, pass_internet);

    Serial.print("Connecting to Internet");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    Serial.println("\nInternet Connected!");

    config.database_url = FIREBASE_HOST;
    config.signer.tokens.legacy_token = FIREBASE_AUTH;

    Firebase.reconnectWiFi(true);
    fbdo.setResponseSize(4096);
    Firebase.begin(&config, &auth);
    
    while (!Firebase.ready()) {
        Serial.print(".");
        delay(100);
    }
    Serial.println("\nFirebase Ready!");

    udp.begin(udpPort);

    // Email callback
    smtp.callback(smtpCallback);

    // Khởi tạo ElegantOTA
    server.on("/", []() {
        server.send(200, "text/plain", "Gateway is running. To update, go to /update");
    });
    ElegantOTA.begin(&server); 
    server.begin();
    Serial.println("HTTP server started for OTA");
}

void sendCommandToSensor(int cmd) {
    if (sensorConnected) {
        cmdData.command = cmd;
        udp.beginPacket(sensorIP, udpPort);
        udp.write((const uint8_t*)&cmdData, sizeof(cmdData));
        udp.endPacket();
        Serial.printf("-> Sent UDP Command: %d\n", cmd);
    }
}

void pushDataToFirebase() {
    if (Firebase.ready()) {
        FirebaseJson monitorJson;
        FirebaseJson alertsJson;
        
        // ĐÃ FIX: Thay 'json' bằng 'monitorJson'
        monitorJson.set("temp", myData.temp_val);
        monitorJson.set("hum", myData.hum_val);
        monitorJson.set("gas_val", myData.mq2_val);
        monitorJson.set("water1", myData.water1_val);
        monitorJson.set("water2", myData.water2_val);
        monitorJson.set("led_motion", myData.led_stt);
        monitorJson.set("servo_angle", myData.servo_ang);

        String statusText = "Normal";
        if (myData.water1_val > 3000) {
            statusText = "FLOOD WARNING!";
            sendWarningEmail("ALARM: Flood Detected!", "The water level has exceeded 3000!");
        } else if (myData.mq2_val > 3000) {
            statusText = "GAS LEAKAGE!";
            sendWarningEmail("ALARM: Gas Leakage!", "High gas concentration detected!");
        } else if (myData.temp_val > 50) {
            statusText = "HIGH TEMP!";
            sendWarningEmail("ALARM: High temp!", "Temperature over 50C detected!");
        } 
        monitorJson.set("status", statusText);

        alertsJson.set("flood", (myData.water1_val > 3000) ? 1 : 0);
        alertsJson.set("gas_leak", (myData.mq2_val > 3000) ? 1 : 0);
        alertsJson.set("high_temp", (myData.temp_val > 50.0) ? 1 : 0);
        alertsJson.set("low_humidity", (myData.hum_val < 20.0) ? 1 : 0);

        // Update RTDB
        Firebase.RTDB.updateNode(&fbdo, "/monitor", &monitorJson);
        Firebase.RTDB.updateNode(&fbdo, "/alerts", &alertsJson);
        Serial.println("Firebase Sync: OK");
    }
}


void checkFirebaseCommand() {
    if (Firebase.ready()) {
        if (Firebase.RTDB.getInt(&fbdo, "/control/cmd")) {
            int cmd = fbdo.intData();
            if (cmd != 0) {
                Serial.printf("Firebase Command Received: %d\n", cmd);

                // OTA
                if (cmd == 4) {
                    Serial.println(">>> OTA MODE ACTIVATED <<<");
                    Serial.print("Go to: http://");
                    Serial.print(WiFi.localIP());
                    Serial.println("/update");

                    // Báo trạng thái lên Firebase để Web hiển thị cho người dùng
                    Firebase.RTDB.setString(&fbdo, "/monitor/status", "OTA READY: " + WiFi.localIP().toString() + "/update");
                }

                // Vẫn gửi lệnh sang Sensor (nếu cần Sensor phản ứng gì đó)
                sendCommandToSensor(cmd);

                // Reset lệnh về 0
                Firebase.RTDB.setInt(&fbdo, "/control/cmd", 0);
            }
        }
    }
}

void smtpCallback(SessionStatus status) {
    Serial.println(status.info());
}

void loop() {
    server.handleClient(); // Xử lý các yêu cầu Web (bắt buộc)
    ElegantOTA.loop();

    int packetSize = udp.parsePacket();
    if (packetSize) {
        sensorIP = udp.remoteIP();
        sensorConnected = true;

        if (packetSize == sizeof(myData)) {
            udp.read((char*)&myData, sizeof(myData));
            pushDataToFirebase();
        } else {
            char flush[255];
            udp.read(flush, packetSize);
        }
    }

    if (millis() - lastFirebaseCheck > 500) {
        lastFirebaseCheck = millis();
        checkFirebaseCommand();
    }
}