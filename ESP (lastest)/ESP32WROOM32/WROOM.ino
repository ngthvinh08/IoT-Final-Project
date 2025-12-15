#include <WiFi.h>
#include <WiFiUdp.h>
#include <Firebase_ESP_Client.h>

// Cung cấp các hàm hỗ trợ tạo Token và tính toán RTDB (Bắt buộc cho thư viện Mobizt)
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// --- 1. CẤU HÌNH WIFI ---
char ssid_internet[] = "Duong192004";
char pass_internet[] = "88888888";

const char *ssid_ap = "ESP32_GATEWAY";
const char *pass_ap = "12345678";

// --- 2. CẤU HÌNH FIREBASE ---
// Lưu ý: Host không có "https://" và không có "/" ở cuối
#define FIREBASE_HOST "iotprj-537a3-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "F824sV1vzGRd5eUfRs7qqAPRckRbGunYuFkGvn6w"

// --- ĐỐI TƯỢNG FIREBASE ---
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// --- CẤU HÌNH UDP ---
WiFiUDP udp;
const int udpPort = 4210;
IPAddress sensorIP;
bool sensorConnected = false;

// --- STRUCT DỮ LIỆU (Giống hệt con Sensor) ---
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
    int command; // 1: Up, 2: Down, 3: Reset
} struct_command;

struct_command cmdData;
struct_message myData;

// Biến timer
unsigned long lastFirebaseCheck = 0;
bool taskCompleted = false;

void setup() {
    Serial.begin(115200);

    // 1. Kết nối Wifi Internet & Phát Wifi AP
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(ssid_ap, pass_ap);
    WiFi.begin(ssid_internet, pass_internet);

    Serial.print("Dang ket noi Internet");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    Serial.println("\nDa ket noi Internet!");
    Serial.print("IP Wan: "); Serial.println(WiFi.localIP());
    Serial.print("IP Lan: "); Serial.println(WiFi.softAPIP());

    // 2. Khởi tạo Firebase
    config.api_key = ""; // Nếu dùng Auth Token thì để trống API Key
    config.database_url = FIREBASE_HOST;
    config.signer.tokens.legacy_token = FIREBASE_AUTH;

    // Tự động kết nối lại khi mất mạng
    Firebase.reconnectWiFi(true);
    
    // Tăng kích thước buffer nhận dữ liệu (cần thiết cho ESP32)
    fbdo.setResponseSize(4096);

    Firebase.begin(&config, &auth);
    
    // Chờ kết nối Firebase
    while (!Firebase.ready()) {
        Serial.print(".");
        delay(100);
    }
    Serial.println("\nFirebase Ready!");

    // 3. Khởi tạo UDP
    udp.begin(udpPort);
    Serial.println("UDP Server Started");
}

// --- HÀM GỬI LỆNH UDP VỀ SENSOR ---
void sendCommandToSensor(int cmd) {
    if (sensorConnected) {
        cmdData.command = cmd;
        udp.beginPacket(sensorIP, udpPort);
        udp.write((const uint8_t*)&cmdData, sizeof(cmdData));
        udp.endPacket();
        Serial.print("-> Da gui lenh UDP: "); Serial.println(cmd);
    } else {
        Serial.println("Loi: Chua co Sensor nao ket noi den Gateway!");
    }
}

// --- HÀM ĐẨY DỮ LIỆU LÊN FIREBASE ---
void pushDataToFirebase() {
    if (Firebase.ready()) {
        // Sử dụng FirebaseJson để đóng gói dữ liệu gọn gàng
        FirebaseJson json;
        
        // Thêm dữ liệu vào JSON
        json.set("temp", myData.temp_val);
        json.set("hum", myData.hum_val);
        json.set("gas_val", myData.mq2_val);
        json.set("water1", myData.water1_val);
        json.set("water2", myData.water2_val);
        json.set("led_motion", myData.led_stt);
        json.set("servo_angle", myData.servo_ang);

        // Tạo trạng thái cảnh báo text
        String statusText = "Normal";
        if (myData.water1_val > 3000) statusText = "LŨ KHẨN CẤP!";
        else if (myData.mq2_val > 3000) statusText = "RÒ RỈ GAS!";
        json.set("status", statusText);

        // Đẩy toàn bộ cục JSON lên nhánh "/monitor"
        // updateNode sẽ không xóa các dữ liệu khác, chỉ cập nhật cái mới
        if (Firebase.RTDB.updateNode(&fbdo, "/monitor", &json)) {
            Serial.println("Da day du lieu len Firebase: OK");
        } else {
            Serial.print("Loi Firebase: ");
            Serial.println(fbdo.errorReason());
        }
        
        // Xử lý Alert riêng (Ví dụ để kích hoạt App khác)
        if (myData.water1_val > 3000) Firebase.RTDB.setInt(&fbdo, "/alerts/flood", 1);
        else Firebase.RTDB.setInt(&fbdo, "/alerts/flood", 0);
    }
}

// --- HÀM KIỂM TRA LỆNH TỪ FIREBASE ---
void checkFirebaseCommand() {
    if (Firebase.ready()) {
        // Đọc giá trị từ đường dẫn "/control/cmd"
        // 0: Ko lam gi, 1: Nang, 2: Ha, 3: Reset
        if (Firebase.RTDB.getInt(&fbdo, "/control/cmd")) {
            if (fbdo.dataType() == "int") {
                int cmd = fbdo.intData();
                
                if (cmd != 0) {
                    Serial.print("Nhan lenh Firebase: "); Serial.println(cmd);
                    
                    // Gửi lệnh qua UDP
                    sendCommandToSensor(cmd);
                    
                    // Reset biến trên Firebase về 0 ngay lập tức
                    // Để tạo hiệu ứng "nút nhấn nhả"
                    Firebase.RTDB.setInt(&fbdo, "/control/cmd", 0);
                }
            }
        }
    }
}

void loop() {
    // 1. NHẬN UDP TỪ SENSOR
    int packetSize = udp.parsePacket();
    if (packetSize) {
        sensorIP = udp.remoteIP();
        sensorConnected = true;

        if (packetSize == sizeof(myData)) {
            udp.read((char*)&myData, sizeof(myData));

            // Log ra Serial để kiểm tra
            Serial.print("Nhan tu Sensor -> T: "); Serial.print(myData.temp_val);
            Serial.print(" | H: "); Serial.print(myData.hum_val);
            Serial.print(" | W: "); Serial.println(myData.water1_val);

            // Đẩy lên Firebase
            pushDataToFirebase();
        } else {
            // Xóa buffer nếu gói tin rác
            char temp[255];
            udp.read(temp, packetSize);
        }
    }

    // 2. KIỂM TRA LỆNH TỪ FIREBASE (Mỗi 500ms check 1 lần)
    if (millis() - lastFirebaseCheck > 500) {
        lastFirebaseCheck = millis();
        checkFirebaseCommand();
    }
}