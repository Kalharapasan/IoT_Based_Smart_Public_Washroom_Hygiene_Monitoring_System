#include <esp_now.h>       
#include <WiFi.h>          
#include <Wire.h>          
#include <LiquidCrystal_I2C.h> 
#include <DHT.h>           
#include <SPI.h>           
#include <SD.h>            
#include <FirebaseESP32.h> 

// --- 1. SETTINGS ---
#define WIFI_SSID      "5T"
#define WIFI_PASSWORD  "11114444"
#define FIREBASE_HOST  "smart-washroom-system-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH  "AIzaSyDn8CicqXCbBxy6jUyYJz_0f-sTu18XJ9E" 

// --- 2. PIN DEFINITIONS ---
#define SD_CS       5      
#define GSM_RX      16     
#define GSM_TX      17     
#define LDR_OUTER   34     
#define LDR_INNER   35     
#define MQ2_PIN     32     
#define DHTPIN      25     
#define PIR_PIN     26     
#define BTN_DISPLAY 4      
#define BTN_RESET   13     
#define LED_RED     12     
#define LED_GREEN   14     
#define BUZZER      27     

// --- 3. GLOBAL VARIABLES ---
float TANK_MAX_LITERS = 500.0; 
float WATER_LOW_LIMIT = 100.0; 
float TANK_H = 100.0;          

int peopleInCount = 0;      
int totalUsageCount = 0;    
int cleanCount = 0;         
int tankFillCount = 0;      
int USAGE_SMS_LIMIT = 5;    
const String SMS_NUMBER = "+94723466524"; 

bool displayActive = false; 
bool usageSmsSent = false;  
bool waterSmsSent = false;  
bool gasSmsSent = false;
bool wasLow = false;        
String lastSmsStatus = "N/A"; 

unsigned long displayStartTime = 0; 
unsigned long lastFirebaseLog = 0;  
unsigned long lastSerialPrint = 0;  

DHT dht(DHTPIN, DHT22);             
LiquidCrystal_I2C lcd(0x27, 16, 2); 
HardwareSerial sim800(2);           

FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

float currentLiters = 0, hum = 0, temp = 0;
int gasValue = 0;
bool motion = false;

typedef struct struct_message {
    float distance;
} struct_message;
struct_message myData;

// --- FUNCTIONS ---

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    memcpy(&myData, incomingData, sizeof(myData));
}

// Function to Send Detailed SMS
bool sendSMS(String msg) {
    Serial.println(">>> GSM: Sending Detailed SMS...");
    sim800.println("AT+CMGF=1"); delay(500);
    sim800.print("AT+CMGS=\""); sim800.print(SMS_NUMBER); sim800.println("\"");
    delay(500); 
    sim800.print(msg); delay(200);
    sim800.write(26); 
    
    unsigned long start = millis();
    bool success = false;
    while (millis() - start < 8000) { 
        if (sim800.find("OK")) { success = true; break; }
    }
    lastSmsStatus = success ? "SENT_OK" : "FAILED";
    return success;
}

// NEW: Function to Sync Pending Data from SD to Firebase
void syncOfflineLogs() {
    if (WiFi.status() == WL_CONNECTED && SD.exists("/offline_data.txt")) {
        Serial.println(">>> WiFi Recovered! Syncing offline logs to Firebase...");
        File offlineFile = SD.open("/offline_data.txt", FILE_READ);
        if (offlineFile) {
            while (offlineFile.available()) {
                String line = offlineFile.readStringUntil('\n');
                if (line.length() > 5) {
                    FirebaseJson json;
                    json.set("Offline_Data", line);
                    Firebase.pushJSON(firebaseData, "/History_Log", json);
                }
            }
            offlineFile.close();
            SD.remove("/offline_data.txt"); // Clear after sync
            Serial.println(">>> Sync Complete. Offline file cleared.");
        }
    }
}

// Log data with Offline Support
void logData(String event) {
    String dataString = "Evt:" + event + " | In:" + String(peopleInCount) + " | Use:" + String(totalUsageCount) + " | Wat:" + String(currentLiters,1) + "L | Gas:" + String(gasValue);
    
    // 1. Permanent Log on SD
    File historyFile = SD.open("/history.txt", FILE_APPEND);
    if (historyFile) {
        historyFile.println(dataString);
        historyFile.close();
    }

    // 2. Firebase Sync or Offline Storage
    if (WiFi.status() == WL_CONNECTED) {
        FirebaseJson json;
        json.set("Event", event);
        json.set("Occupancy", peopleInCount);
        json.set("Total_Usage", totalUsageCount);
        json.set("Water_Level", currentLiters); 
        json.set("Gas_Level", gasValue);
        json.set("Temp", temp);
        json.set("SMS_Status", lastSmsStatus);
        Firebase.updateNode(firebaseData, "/Live_Status", json);
        Firebase.pushJSON(firebaseData, "/History_Log", json);
    } else {
        // WiFi is down, save to special offline file
        File offlineFile = SD.open("/offline_data.txt", FILE_APPEND);
        if (offlineFile) {
            offlineFile.println(dataString);
            offlineFile.close();
            Serial.println("WiFi Down: Data logged to Offline file on SD.");
        }
    }
}

// Function to read and show existing SD data on Boot
void readHistoryFromSD() {
    Serial.println("--- READING PREVIOUS DATA FROM SD ---");
    File dataFile = SD.open("/history.txt");
    if (dataFile) {
        while (dataFile.available()) {
            Serial.write(dataFile.read());
        }
        dataFile.close();
    } else {
        Serial.println("No previous history found on SD Card.");
    }
    Serial.println("--------------------------------------");
}

// --- SETUP ---
void setup() {
    Serial.begin(115200); 
    sim800.begin(9600, SERIAL_8N1, GSM_RX, GSM_TX);
    
    Wire.begin(21, 22); 
    lcd.init(); lcd.backlight();
    lcd.clear(); lcd.print("HARDWARE TEST...");
    
    pinMode(LDR_OUTER, INPUT); pinMode(LDR_INNER, INPUT);
    pinMode(PIR_PIN, INPUT); pinMode(MQ2_PIN, INPUT);
    pinMode(BTN_DISPLAY, INPUT_PULLUP); pinMode(BTN_RESET, INPUT_PULLUP);
    pinMode(LED_RED, OUTPUT); pinMode(LED_GREEN, OUTPUT);
    pinMode(BUZZER, OUTPUT);

    dht.begin();
    
    // Test SD Card and Read History
    if (SD.begin(SD_CS)) {
        Serial.println("SD Card: OK");
        readHistoryFromSD(); 
    } else {
        Serial.println("SD Card: FAILED");
    }

    // Network Connection
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 15) {
        delay(500); Serial.print("."); retry++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        config.host = FIREBASE_HOST;
        config.signer.tokens.legacy_token = FIREBASE_AUTH;
        Firebase.begin(&config, &auth);
        Firebase.reconnectWiFi(true);
        syncOfflineLogs(); // Sync any data missed before reboot
    }

    WiFi.mode(WIFI_AP_STA);
    if (esp_now_init() == ESP_OK) {
        esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
    }

    lcd.clear(); lcd.print("SYSTEM READY");
    delay(2000); lcd.noBacklight();
}

void loop() {
    // 1. DATA COLLECTION
    float waterLevelCm = TANK_H - myData.distance; 
    if (waterLevelCm < 0) waterLevelCm = 0;
    currentLiters = (waterLevelCm / TANK_H) * TANK_MAX_LITERS;

    gasValue = analogRead(MQ2_PIN); 
    temp = dht.readTemperature();   
    hum = dht.readHumidity();      
    motion = digitalRead(PIR_PIN); 

    // 2. DETAILED SMS LOGIC
    if (currentLiters < WATER_LOW_LIMIT && !waterSmsSent) {
        String msg = "WATER ALERT! Tank is low: " + String(currentLiters, 1) + "L left. Refill needed.";
        if(sendSMS(msg)) { waterSmsSent = true; logData("WATER_LOW"); }
    }
    if (currentLiters > 400.0) waterSmsSent = false; 

    if (gasValue > 2500 && !gasSmsSent) {
        String msg = "GAS ALERT! High smell detected: " + String(gasValue) + ". Check ventilation.";
        if(sendSMS(msg)) { gasSmsSent = true; logData("GAS_EMERGENCY"); }
    }
    if (gasValue < 2000) gasSmsSent = false;

    if (totalUsageCount >= USAGE_SMS_LIMIT && !usageSmsSent) {
        String msg = "CLEANING REQ! Total Usage: " + String(totalUsageCount) + " people. Please clean now.";
        if(sendSMS(msg)) { usageSmsSent = true; logData("CLEAN_REQ"); }
    }

    // 3. PEOPLE COUNTER
    static int lastOuter = 4095, lastInner = 4095;
    int curOuter = analogRead(LDR_OUTER);
    int curInner = analogRead(LDR_INNER);

    if (curOuter < 1500 && lastOuter >= 1500) { 
        totalUsageCount++; peopleInCount++;
        logData("USER_ENTRY");
        delay(500);
    }
    if (curInner < 1500 && lastInner >= 1500) { 
        if (peopleInCount > 0) peopleInCount--;
        logData("USER_EXIT");
        delay(500);
    }
    lastOuter = curOuter; lastInner = curInner;

    // 4. LED & BUZZER INDICATORS
    if (totalUsageCount >= USAGE_SMS_LIMIT || gasValue > 2500) {
        digitalWrite(LED_RED, HIGH); digitalWrite(LED_GREEN, LOW);
        digitalWrite(BUZZER, HIGH);
    } else {
        digitalWrite(LED_RED, LOW); digitalWrite(LED_GREEN, HIGH);
        digitalWrite(BUZZER, LOW);
    }

    // 5. RESET BUTTON
    if (digitalRead(BTN_RESET) == LOW) {
        cleanCount++; peopleInCount = 0; totalUsageCount = 0; 
        usageSmsSent = false; gasSmsSent = false;
        logData("SYSTEM_CLEANED");
        lcd.backlight(); lcd.clear(); lcd.print("CLEAN RECORDED"); 
        delay(1500); lcd.noBacklight();
    }

    // 6. SERIAL MONITORING (Original Detailed Format)
    if (millis() - lastSerialPrint > 2000) {
        Serial.print(">>> STATUS | ");
        Serial.print("In: " + String(peopleInCount) + " | ");
        Serial.print("Total: " + String(totalUsageCount) + " | ");
        Serial.print("Water: " + String(currentLiters, 1) + "L | ");
        Serial.print("Gas: " + String(gasValue) + " | ");
        Serial.print("Temp: " + String(temp, 1) + "C | ");
        Serial.println("SMS: " + lastSmsStatus);
        lastSerialPrint = millis();
    }

    // 7. FIREBASE SYNC & OFFLINE CHECK
    if (millis() - lastFirebaseLog > 10000) {
        syncOfflineLogs(); // Check if internet is back and sync logs
        logData("PERIODIC_SYNC");
        lastFirebaseLog = millis();
    }

    // 8. LCD DISPLAY CONTROL (Original Format)
    if (digitalRead(BTN_DISPLAY) == LOW) { 
        displayActive = true; 
        displayStartTime = millis();
        lcd.backlight();
    }

    if (displayActive) {
        if (millis() - displayStartTime > 15000) {
            displayActive = false; lcd.noBacklight(); lcd.clear();
        } else {
            int toggle = (millis() / 3000) % 4;
            lcd.setCursor(0,0);
            if (toggle == 0) {
                lcd.print("In:"); lcd.print(peopleInCount); lcd.print(" Clean:"); lcd.print(cleanCount);
                lcd.setCursor(0,1); lcd.print("Usage:"); lcd.print(totalUsageCount);
            } else if (toggle == 1) {
                lcd.print("Water: "); lcd.print((int)currentLiters); lcd.print(" L");
                lcd.setCursor(0,1); lcd.print("Status: "); lcd.print(waterSmsSent?"LOW":"OK");
            } else if (toggle == 2) {
                lcd.print("SMS Last Stat:");
                lcd.setCursor(0,1); lcd.print(lastSmsStatus);
            } else {
                lcd.print("G:"); lcd.print(gasValue); lcd.print(" T:"); lcd.print(temp,0);
                lcd.setCursor(0,1); lcd.print("M:"); lcd.print(motion?"YES":"NO"); lcd.print(" H:"); lcd.print(hum,0);
            }
        }
    }
    delay(50);
}