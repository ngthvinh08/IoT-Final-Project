#include <WiFi.h>
#include <WiFiUdp.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <Update.h>


WebServer server(80); 


const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>HomeX | Web Monitor</title>
    <link href="https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;700&display=swap" rel="stylesheet">
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
    <style>
        :root {
            --bg-color: #2c3e50; --card-bg: #34495e; --text-main: #81ecec; --text-light: #ecf0f1;
            --accent-color: #95a5a6; --success: #55efc4; --warning: #ffeaa7; --danger: #ff7675; --purple: #a29bfe;
        }
        body { font-family: 'Roboto', sans-serif; background-color: var(--bg-color); color: var(--text-light); margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; min-height: 100vh; }
        h1 { margin-bottom: 30px; color: var(--text-main); text-align: center; text-transform: uppercase; letter-spacing: 2px; font-weight: 700; }
        .container { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 20px; width: 100%; max-width: 1000px; }
        .card { background-color: var(--card-bg); border-radius: 16px; padding: 25px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); text-align: center; transition: transform 0.3s ease; border: 1px solid rgba(255,255,255,0.05); }
        .card:hover { transform: translateY(-5px); }
        .card h2 { margin-top: 0; font-size: 1.1rem; color: var(--text-light); margin-bottom: 15px; font-weight: 400; text-transform: uppercase; opacity: 0.9;}
        .value-box { font-size: 3rem; font-weight: bold; margin: 10px 0; color: var(--text-light); }
        .unit { font-size: 1.2rem; color: var(--accent-color); margin-left: 5px; }
        .status-box { font-size: 1.5rem; font-weight: bold; padding: 15px; border-radius: 12px; background: rgba(0,0,0,0.1); text-transform: uppercase; margin-top: 10px; color: var(--text-light); }
        .bulb-icon { font-size: 4rem; transition: all 0.3s ease-in-out; }
        .bulb-on { color: var(--warning); text-shadow: 0 0 20px rgba(255, 234, 167, 0.5); }
        .bulb-off { color: var(--accent-color); opacity: 0.5; }
        .control-panel { margin-top: 30px; padding: 30px; background: var(--card-bg); border-radius: 16px; width: 100%; max-width: 940px; box-sizing: border-box; }
        .control-panel h2 { text-align: center; margin-top: 0; color: var(--text-light); border-bottom: 1px solid rgba(255,255,255,0.1); padding-bottom: 15px; }
        .btn-group { display: flex; justify-content: center; gap: 20px; flex-wrap: wrap; margin-top: 20px; }
        button { padding: 15px 30px; border: none; border-radius: 12px; font-size: 1rem; font-weight: bold; cursor: pointer; color: var(--card-bg); display: flex; align-items: center; gap: 10px; min-width: 160px; justify-content: center; }
        .btn-up { background-color: var(--success); } .btn-down { background-color: var(--warning); }
        .btn-reset { background-color: var(--danger); color: var(--text-light); } .btn-light { background-color: var(--purple); color: var(--text-light); }
        .sensor-icon { font-size: 2.5rem; margin-bottom: 10px; color: var(--text-main); }
        .alert-active { animation: blinkRed 1s infinite; background-color: var(--danger) !important; color: var(--text-light) !important; }
        .safe-active { background-color: var(--success) !important; color: var(--card-bg) !important; }
        @keyframes blinkRed { 0% { opacity: 1; } 50% { opacity: 0.7; } 100% { opacity: 1; } }
        .footer { margin-top: 40px; color: var(--accent-color); font-size: 0.8rem; }
        .file-input-container { margin: 15px 0; }
        input[type="file"] { color: var(--accent-color); background: rgba(0,0,0,0.1); padding: 10px; border-radius: 8px; width: 90%; border: 1px dashed var(--text-main); }
        .btn-ota { background-color: var(--text-main); color: var(--card-bg); margin: 0 auto; }
        .progress-container { width: 100%; background-color: rgba(0,0,0,0.1); border-radius: 8px; margin-top: 15px; height: 20px; overflow: hidden; display: none; }
        .progress-bar { width: 0%; height: 100%; background-color: var(--success); text-align: center; line-height: 20px; font-size: 0.8rem; color: var(--card-bg); transition: width 0.3s; }
    </style>
</head>
<body>
    <h1><i class="fas fa-shield-halved"></i> MY HOME </h1>
    <div class="container">
        <div class="card">
            <i class="fas fa-water sensor-icon" style="color: #74b9ff;"></i>
            <h2>FLOOD LEVEL</h2>
            <div id="status-water" class="status-box">Loading...</div>
            <div style="margin-top: 10px; font-size: 0.9rem; opacity: 0.7;">Sensor Raw: <span id="water-raw">--</span></div>
        </div>
        <div class="card">
            <i class="fas fa-fire-extinguisher sensor-icon" style="color: #ff7675;"></i>
            <h2>GAS LEVEL</h2>
            <div id="status-gas" class="status-box">Loading...</div>
            <div style="margin-top: 10px; font-size: 0.9rem; opacity: 0.7;">Sensor Raw: <span id="gas-raw">--</span></div>
        </div>
        <div class="card">
            <i class="fas fa-temperature-high sensor-icon" style="color: var(--text-main);"></i>
            <h2>Temperature</h2>
            <div class="value-box"><span id="temp">--</span> <span class="unit">°C</span></div>
        </div>
        <div class="card">
            <i class="fas fa-droplet sensor-icon" style="color: var(--text-main);"></i>
            <h2>Humidity</h2>
            <div class="value-box"><span id="hum">--</span> <span class="unit">%</span></div>
        </div>
        <div class="card">
            <i class="fas fa-robot sensor-icon" style="color: var(--text-main);"></i>
            <h2>Barrier Status</h2>
            <div class="value-box"><span id="servo">--</span> <span class="unit">Deg</span></div>
        </div>
        <div class="card">
            <h2>Hallway Light</h2>
            <div class="value-box"><i id="bulb-status" class="fas fa-lightbulb bulb-icon bulb-off"></i></div>
            <div id="bulb-text" style="font-size: 0.9rem; margin-top: 5px; opacity: 0.7;">OFF</div>
        </div>
        <div class="card" style="border: 1px solid var(--text-main);">
            <i class="fas fa-microchip sensor-icon" style="color: var(--text-main);"></i>
            <h2>SYSTEM UPDATE</h2>
            <div class="file-input-container"><input type="file" id="otafile" name="update" accept=".bin"></div>
            <button class="btn-ota" onclick="startOTA()"><i class="fas fa-upload"></i> UPLOAD FIRMWARE</button>
            <div class="progress-container" id="prg-container"><div class="progress-bar" id="prg-bar">0%</div></div>
            <div id="ota-msg" style="margin-top:10px; font-size: 0.9rem; color: var(--accent-color);"></div>
        </div>
    </div>
    <div class="control-panel">
        <h2><i id="remote-icon" class="fas fa-gamepad"></i> MANUAL CONTROL</h2>
        <div class="btn-group">
            <button class="btn-up" onclick="sendCommand(1)"><i class="fas fa-arrow-up"></i> RAISE</button>
            <button class="btn-down" onclick="sendCommand(2)"><i class="fas fa-arrow-down"></i> LOWER</button>
            <button class="btn-reset" onclick="sendCommand(3)"><i class="fas fa-power-off"></i> RESET</button>
            <button class="btn-light" onclick="sendCommand(5)"><i class="fas fa-lightbulb"></i> LIGHT ON</button>
        </div>
    </div>
    <div class="footer">Smart Home IoT Dashboard v2.1 (Pastel Theme)</div>
    <script>
        function startOTA() {
            var fileInput = document.getElementById('otafile');
            var msgBox = document.getElementById('ota-msg');
            var prgContainer = document.getElementById('prg-container');
            var prgBar = document.getElementById('prg-bar');
            if (fileInput.files.length === 0) { msgBox.style.color = 'var(--danger)'; msgBox.innerText = "Please select a .bin file first!"; return; }
            var file = fileInput.files[0];
            var formData = new FormData();
            formData.append("update", file);
            prgContainer.style.display = 'block';
            msgBox.innerText = "Uploading...";
            msgBox.style.color = 'var(--accent-color)';
            var xhr = new XMLHttpRequest();
            xhr.upload.addEventListener("progress", function(e) {
                if (e.lengthComputable) {
                    var percent = Math.round((e.loaded / e.total) * 100);
                    prgBar.style.width = percent + "%";
                    prgBar.innerText = percent + "%";
                }
            }, false);
            xhr.onload = function() {
                if (xhr.status === 200) { prgBar.style.backgroundColor = "var(--success)"; msgBox.style.color = "var(--success)"; msgBox.innerText = "Update Complete. Rebooting..."; }
                else { msgBox.style.color = "var(--danger)"; msgBox.innerText = "Update Failed."; }
            };
            xhr.onerror = function() { msgBox.style.color = "var(--danger)"; msgBox.innerText = "Connection Error!"; };
            xhr.open("POST", "/update");
            xhr.send(formData);
        }
    </script>
    <script type="module">
        import { initializeApp } from "https://www.gstatic.com/firebasejs/9.22.0/firebase-app.js";
        import { getDatabase, ref, onValue, set } from "https://www.gstatic.com/firebasejs/9.22.0/firebase-database.js";
        const firebaseConfig = {
            apiKey: "AIzaSyD5GdJpxPJzuwCuyax6jpIk-_YtfxlOxuw",
            authDomain: "iotprj-537a3.firebaseapp.com",
            databaseURL: "https://iotprj-537a3-default-rtdb.asia-southeast1.firebasedatabase.app",
            projectId: "iotprj-537a3",
            storageBucket: "iotprj-537a3.firebasestorage.app",
            messagingSenderId: "763414029810",
            appId: "1:763414029810:web:87f7f84f95ed2dc574b76b"
        };
        const app = initializeApp(firebaseConfig);
        const db = getDatabase(app);
        const monitorRef = ref(db, 'monitor');
        onValue(monitorRef, (snapshot) => {
            const data = snapshot.val();
            if (data) {
                document.getElementById('temp').innerText = data.temp ? data.temp.toFixed(1) : "--";
                document.getElementById('hum').innerText = data.hum ? data.hum.toFixed(1) : "--";
                document.getElementById('servo').innerText = data.servo_angle;
                document.getElementById('gas-raw').innerText = data.gas_val;
                document.getElementById('water-raw').innerText = data.water1;
                const waterBox = document.getElementById('status-water');
                if (data.water1 > 3000) { waterBox.innerText = "DANGER"; waterBox.className = "status-box alert-active"; }
                else { waterBox.innerText = "NORMAL"; waterBox.className = "status-box safe-active"; }
                const gasBox = document.getElementById('status-gas');
                if (data.gas_val > 3000) { gasBox.innerText = "LEAK DETECTED"; gasBox.className = "status-box alert-active"; }
                else { gasBox.innerText = "SAFE"; gasBox.className = "status-box safe-active"; }
                const bulbEl = document.getElementById('bulb-status');
                const bulbText = document.getElementById('bulb-text');
                if (data.led_motion == 1) { bulbEl.className = "fas fa-lightbulb bulb-icon bulb-on"; bulbText.innerText = "ON"; bulbText.style.color = "var(--warning)"; }
                else { bulbEl.className = "fas fa-lightbulb bulb-icon bulb-off"; bulbText.innerText = "OFF"; bulbText.style.color = "var(--accent-color)"; }
            }
        });
        window.sendCommand = function(cmdCode) {
            const iconEl = document.getElementById('remote-icon');
            set(ref(db, 'control/cmd'), cmdCode)
            .then(() => {
                iconEl.style.color = "var(--success)";
                iconEl.style.transform = "scale(1.2)";
                setTimeout(() => { iconEl.style.color = "var(--text-main)"; iconEl.style.transform = "scale(1)"; }, 500);
            })
            .catch((error) => { console.error("Command error:", error); iconEl.style.color = "var(--warning)"; });
        }
    </script>
</body>
</html>
)rawliteral";


String email_script = "https://script.google.com/macros/s/AKfycbxWl4hSZ9G7NnRMQVCj_t_WIIcAJeUpdgLVFTUtBBnE6qkuemcl1mpIOyfDAfDKA5xM/exec";


const char *ssid_ap = "ESP32_GATEWAY";
const char *pass_ap = "12345678";


#define FIREBASE_HOST "iotprj-537a3-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "F824sV1vzGRd5eUfRs7qqAPRckRbGunYuFkGvn6w"

#define RESET_AP_CONFIG 1


FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;


WiFiUDP udp;
const int udpPort = 4210;

IPAddress sensorIP;
bool sensorConnected = false;


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
unsigned long lastEmailTime_Flood = 0;
unsigned long lastEmailTime_Gas = 0;
const long emailInterval = 300000;


void sendEmail(String type) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = email_script;
    url += "?type=";
    url += type;
    Serial.println(">> Gateway: Dang gui email...");
    http.begin(url.c_str());
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int httpCode = http.GET();
    http.end();
  }
}


void checkAlerts() {
  if (myData.water1_val > 3000 && (millis() - lastEmailTime_Flood > emailInterval)) {
    sendEmail("flood");
    lastEmailTime_Flood = millis();
  }
  if (myData.mq2_val > 3000 && (millis() - lastEmailTime_Gas > emailInterval)) {
    sendEmail("gas");
    lastEmailTime_Gas = millis();
  }
}


void setup() {
  Serial.begin(115200);


  WiFiManager wm;

  #if RESET_AP_CONFIG 
  wm.resetSettings(); 
  #endif

  bool res = wm.autoConnect("GATEWAY_CONFIG", "12345678");
  if(!res) { Serial.println("Ket noi that bai. Restarting..."); ESP.restart(); }
  else { Serial.println("WiFiManager connected!"); }


  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssid_ap, pass_ap);
 
  Serial.print("IP Internet (Dung de truy cap Web OTA): "); Serial.println(WiFi.localIP());
  Serial.print("IP Gateway (cho Sensor): "); Serial.println(WiFi.softAPIP());


  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", index_html);
  });


  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { Update.printError(Serial); }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) { Update.printError(Serial); }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize); }
      else { Update.printError(Serial); }
    }
  });


  server.begin();
  Serial.println("Web Server & OTA Started");

  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);
  Firebase.begin(&config, &auth);

  udp.begin(udpPort);
  Serial.println("UDP Server Started");
}


void sendCommandToSensor(int cmd) {
  if (sensorConnected) {
    cmdData.command = cmd;
    udp.beginPacket(sensorIP, udpPort);
    udp.write((const uint8_t *)&cmdData, sizeof(cmdData));
    udp.endPacket();
    Serial.printf("-> Gui lenh UDP: %d\n", cmd);
  }
}


void pushDataToFirebase() {
  if (Firebase.ready()) {
    FirebaseJson json;
    json.set("temp", myData.temp_val);
    json.set("hum", myData.hum_val);
    json.set("gas_val", myData.mq2_val);
    json.set("water1", myData.water1_val);
    json.set("water2", myData.water2_val);
    json.set("led_motion", myData.led_stt);
    json.set("servo_angle", myData.servo_ang);
    String statusText = (myData.water1_val > 3000) ? "LŨ KHẨN CẤP!" : (myData.mq2_val > 3000 ? "RÒ RỈ GAS!" : "Normal");
    json.set("status", statusText);
    Firebase.RTDB.updateNode(&fbdo, "/monitor", &json);
  }
}


void checkFirebaseCommand() {
  if (Firebase.ready()) {
    if (Firebase.RTDB.getInt(&fbdo, "/control/cmd")) {
      if (fbdo.dataType() == "int") {
        int cmd = fbdo.intData();
        if (cmd != 0) {
          if (sensorConnected) sendCommandToSensor(cmd);
          Firebase.RTDB.setInt(&fbdo, "/control/cmd", 0);
          if (cmd == 3) { lastEmailTime_Flood = 0; lastEmailTime_Gas = 0; }
        }
      }
    }
  }
}


void loop() {
  server.handleClient(); 

  int packetSize = udp.parsePacket();
  if (packetSize == sizeof(myData)) {
    sensorIP = udp.remoteIP();
    sensorConnected = true;
    udp.read((char *)&myData, sizeof(myData));
    checkAlerts();
    pushDataToFirebase();
  }


  if (millis() - lastFirebaseCheck > 500) {
    lastFirebaseCheck = millis();
    checkFirebaseCommand();
  }
}

