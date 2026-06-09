#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <DHT.h>

// --- 1. 硬件引脚定义 ---
#define BUZZER_PIN PA0
#define DHT_PIN    PB1
#define DHT_TYPE   DHT11
#define SDA_PIN    PB7
#define SCL_PIN    PB6

HardwareSerial EspSerial(PA3, PA2); 
Adafruit_PN532 nfc(SCL_PIN, SDA_PIN);
DHT dht(DHT_PIN, DHT_TYPE);

// --- 2. WiFi 与 OneNet 核心配置 ---
const char* WIFI_SSID = "zhang";      
const char* WIFI_PASS = "zhangjiaqian"; 

const char* ONENET_PID  = "6rvH8d9yQ2"; 
const char* ONENET_DN   = "esp222";     

const char* ONENET_TOKEN = "version=2018-10-31&res=products%2F6rvH8d9yQ2%2Fdevices%2Fesp222&et=2090465158&method=md5&sign=aaC%2FTd3CdHWlNTgZIjx0SQ%3D%3D"; 

const char* PUB_TOPIC = "$sys/6rvH8d9yQ2/esp222/thing/property/post";

bool isCloudConnected = false;

// --- 3. 稳健发送 AT 指令 ---
bool sendAT(String cmd, const char* expected, uint32_t timeout) {
    uint8_t retry = 2;
    while(retry--) {
        EspSerial.println(cmd);
        uint32_t start = millis();
        String response = "";
        while (millis() - start < timeout) {
            while (EspSerial.available()) {
                response += (char)EspSerial.read();
            }
            if (response.indexOf(expected) != -1) return true;
            if (response.indexOf("ERROR") != -1) break;
        }
        delay(500); 
    }
    return false;
}

// --- 4. 建立 OneNet 连接 ---
void connectOneNet() {
    isCloudConnected = false;
    Serial.println("\n>>> Step 0: Resetting ESP8266...");
    EspSerial.println("AT+RST"); 
    delay(3000); 

    // 设置模式与 DHCP
    sendAT("AT+CWMODE=1", "OK", 1000);
    sendAT("AT+CWDHCP=1,1", "OK", 1000);

    Serial.println(">>> Step 1: Connecting WiFi...");
    if (!sendAT("AT+CWJAP=\"" + String(WIFI_SSID) + "\",\"" + String(WIFI_PASS) + "\"", "OK", 10000)) return;

    Serial.println(">>> Step 2: Configuring MQTT User...");
    sendAT("AT+MQTTCLEAN=0", "OK", 1000);
    String cfgCmd = "AT+MQTTUSERCFG=0,1,\"" + String(ONENET_DN) + "\",\"" + String(ONENET_PID) + "\",\"" + String(ONENET_TOKEN) + "\",0,0,\"\"";
    if (!sendAT(cfgCmd, "OK", 5000)) return;

    Serial.println(">>> Step 3: Connecting to Cloud...");
    if (sendAT("AT+MQTTCONN=0,\"mqtts.heclouds.com\",1883,1", "OK", 10000)) {
        Serial.println(">>> [SUCCESS] OneNet Online!");
        isCloudConnected = true;

        // --- 新增：按照你的参考指令订阅 Topic ---
        sendAT("AT+MQTTSUB=0,\"$sys/6rvH8d9yQ2/esp222/thing/property/post/reply\",0", "OK", 1000);
        sendAT("AT+MQTTSUB=0,\"$sys/6rvH8d9yQ2/esp222/thing/property/set\",0", "OK", 1000);
    }
}

// --- 5. OneJSON 数据上报 (重点修正版) ---
void uploadOneJSON(float temp, float humi, String rfid) {
    if (!isCloudConnected) return;

    /* 老师傅笔记：我们要模仿你成功的这条指令：
      "{\"id\":\"123\"\,\"params\":{\"humidity_value\":{\"value\":60\}\,\"temp_value\":{\"value\":11\}}}"
      
      在代码中，\\\" 会发出 \"，\\\, 会发出 \,
    */
    String json = "{";
    json += "\\\"id\\\":\\\"123\\\"\\\,"; // 注意这里的转义逗号
    json += "\\\"params\\\":{";
    json += "\\\"humidity_value\\\":{\\\"value\\\":" + String(humi, 1) + "}\\\,";
    json += "\\\"temp_value\\\":{\\\"value\\\":" + String(temp, 1) + "}\\\,";
    json += "\\\"NFC_ID\\\":{\\\"value\\\":\\\"" + rfid + "\\\"}";
    json += "}}";

    // 组合成完整的 PUB 指令
    String pubCmd = "AT+MQTTPUB=0,\"" + String(PUB_TOPIC) + "\",\"" + json + "\",0,0";
    
    Serial.println(">>> Sending OneJSON Data...");
    if (sendAT(pubCmd, "OK", 3000)) {
        Serial.println(">>> [SUCCESS] Uploaded!");
          Serial.println("Check Cmd: " + pubCmd);
    } else {
        Serial.println(">>> [FAIL] Upload Failed.");
        // 如果失败，可以取消下面注释查看生成的原始指令是否正确
      
    }
}

void setup() {
    Serial.begin(115200);
    EspSerial.begin(115200); 
    Wire.setSDA(SDA_PIN); Wire.setSCL(SCL_PIN); Wire.begin();
    dht.begin(); nfc.begin();
    pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, HIGH);

    connectOneNet();
    if (nfc.getFirmwareVersion()) nfc.SAMConfig();
}

void loop() {
    static unsigned long lastPostTime = 0;
    static String lastUID = "None";

    if (isCloudConnected && (millis() - lastPostTime >= 3000)) {
        lastPostTime = millis();
        float h = dht.readHumidity();
        float t = dht.readTemperature();

        if (!isnan(h) && !isnan(t)) {
            // 报警逻辑
            if (t > 30.0) digitalWrite(BUZZER_PIN, LOW);
            else digitalWrite(BUZZER_PIN, HIGH);

            uploadOneJSON(t, h, lastUID);
        }
    }

    // NFC 刷卡逻辑
    uint8_t uid[] = {0,0,0,0,0,0,0}; uint8_t len;
    if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &len, 50)) {
        lastUID = "";
        for (uint8_t i = 0; i < len; i++) {
            if (uid[i] < 0x10) lastUID += "0";
            lastUID += String(uid[i], HEX);
        }
        digitalWrite(BUZZER_PIN, LOW); delay(100); digitalWrite(BUZZER_PIN, HIGH);
    }

    // 断线重连
    if (!isCloudConnected && (millis() % 60000 < 100)) connectOneNet();
}