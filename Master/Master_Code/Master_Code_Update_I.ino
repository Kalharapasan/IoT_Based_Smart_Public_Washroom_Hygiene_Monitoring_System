#include <esp_now.h>       // For ESP-NOW communication (Water level data)
#include <WiFi.h>          // For WiFi connectivity
#include <Wire.h>          // For I2C communication (LCD)
#include <LiquidCrystal_I2C.h> // For LCD display control
#include <DHT.h>           // For Humidity and Temperature sensor
#include <SPI.h>           // For SPI communication (SD Card)
#include <SD.h>            // For SD Card storage
#include <FirebaseESP32.h> // For Firebase Realtime Database

// --- 1. NETWORK SETTINGS ---
#define WIFI_SSID      "5T"
#define WIFI_PASSWORD  "11114444"
#define FIREBASE_HOST  "smart-washroom-system-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH  "AIzaSyDn8CicqXCbBxy6jUyYJz_0f-sTu18XJ9E" 

// --- 2. UPDATE INTERVALS ---
unsigned long FIREBASE_UPDATE_INTERVAL = 1000; 
unsigned long SERIAL_PRINT_INTERVAL = 1000;    // Increased speed to 1 second for better monitoring

// --- 3. PIN DEFINITIONS ---
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
#define LED_GREEN   14     // Green LED for safe status
#define BUZZER      27     

// --- 4. GLOBAL VARIABLES ---
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
bool wasLow = false;        
String lastSmsStatus = "N/A"; 

unsigned long displayStartTime = 0; 
unsigned long lastFirebaseLog = 0;  
unsigned long lastSerialPrint = 0;  

DHT dht(DHTPIN, DHT22);             
// Try address 0x27 or 0x3F if LCD is not working
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

// ESP-NOW Receive Callback
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    memcpy(&myData, incomingData, sizeof(myData));
}

// Log data to SD Card
void logToSD(String event, String details) {
    File dataFile = SD.open("/data_log.csv", FILE_APPEND);
    if (dataFile) {
        String data = String(millis()/1000) + "," + event + "," + details + ",SMS:" + lastSmsStatus;
        dataFile.println(data);
        dataFile.close();
    }
}

// Send SMS and verify response
bool sendSMS(String msg) {
    Serial.println("GSM: Sending SMS...");
    sim800.println("AT+CMGF=1"); delay(500);
    sim800.print("AT+CMGS=\""); sim800.print(SMS_NUMBER); sim800.println("\"");
    delay(500); 
    sim800.print(msg); delay(200);
    sim800.write(26); 
    
    unsigned long start = millis();
    bool success = false;
    while (millis() - start < 5000) {
        if (sim800.find("OK")) { success = true; break; }
    }
    lastSmsStatus = success ? "SENT_OK" : "FAILED";
    return success;
}

// Sync data with Firebase
void updateFirebase(String event) {
    if (WiFi.status() == WL_CONNECTED) {
        FirebaseJson json;
        json.set("Event", event);
        json.set("P_In", peopleInCount);
        json.set("Usage", totalUsageCount);
        json.set("Water_L", currentLiters); 
        json.set("Gas", gasValue);
        json.set("Temp", temp);
        json.set("SMS_Stat", lastSmsStatus);
        
        Firebase.updateNode(firebaseData, "/Live_Status", json);
        Firebase.pushJSON(firebaseData, "/History", json);
    }
}

// --- MAIN SETUP ---
void setup() {
    Serial.begin(115200); // Increased Baud Rate for better Serial Monitoring
    sim800.begin(9600, SERIAL_8N1, GSM_RX, GSM_TX);
    
    // Initialize LCD with Error Check
    Wire.begin(21, 22); // Explicitly setting SDA/SCL pins
    lcd.init(); 
    lcd.backlight();
    lcd.clear();
    lcd.print("SYSTEM BOOTING");
    Serial.println("--- System Booting ---");
    delay(1000);

    // 1. Testing Sensors & SD
    lcd.clear(); lcd.print("TESTING SENSORS");
    dht.begin();
    pinMode(LDR_OUTER, INPUT); pinMode(LDR_INNER, INPUT);
    pinMode(PIR_PIN, INPUT); pinMode(MQ2_PIN, INPUT);
    pinMode(BTN_DISPLAY, INPUT_PULLUP); pinMode(BTN_RESET, INPUT_PULLUP);
    pinMode(LED_RED, OUTPUT); pinMode(LED_GREEN, OUTPUT);
    pinMode(BUZZER, OUTPUT);
    
    if (SD.begin(SD_CS)) {
        lcd.setCursor(0,1); lcd.print("SD CARD: OK");
        Serial.println("SD Card: Initialized Successfully");
    } else {
        lcd.setCursor(0,1); lcd.print("SD CARD: FAIL");
        Serial.println("SD Card: Initialization Failed!");
    }
    delay(1500);

    // 2. Testing WiFi
    lcd.clear(); lcd.print("WiFi Connecting");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20) {
        delay(500); lcd.print("."); 
        Serial.print("."); retry++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        lcd.setCursor(0,1); lcd.print("WiFi: CONNECTED");
        Serial.println("\nWiFi Connected: " + WiFi.localIP().toString());
    } else {
        lcd.setCursor(0,1); lcd.print("WiFi: TIMEOUT");
        Serial.println("\nWiFi Connection Failed!");
    }
    delay(1500);

    // 3. Testing Firebase
    lcd.clear(); lcd.print("FB CONNECTING...");
    config.host = FIREBASE_HOST;
    config.signer.tokens.legacy_token = FIREBASE_AUTH;
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    
    if (Firebase.ready()) {
        lcd.setCursor(0,1); lcd.print("FIREBASE: READY");
        Serial.println("Firebase: Connected Successfully");
    } else {
        lcd.setCursor(0,1); lcd.print("FIREBASE: ERROR");
        Serial.println("Firebase: Connection Error!");
    }
    delay(1500);

    // 4. ESP-NOW
    WiFi.mode(WIFI_AP_STA);
    if (esp_now_init() == ESP_OK) {
        esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
        Serial.println("ESP-NOW: Initialized");
    }

    digitalWrite(BUZZER, HIGH); delay(300); digitalWrite(BUZZER, LOW);
    lcd.clear(); lcd.print("ALL SYSTEMS OK");
    lcd.setCursor(0,1); lcd.print("READY TO WORK");
    Serial.println("System Ready!");
    delay(3000);
    
    lcd.noBacklight();
    lcd.clear();
}

void loop() {
    // --- 1. WATER MONITORING ---
    float waterLevelCm = TANK_H - myData.distance; 
    if (waterLevelCm < 0) waterLevelCm = 0;
    currentLiters = (waterLevelCm / TANK_H) * TANK_MAX_LITERS;

    if (currentLiters < WATER_LOW_LIMIT) {
        wasLow = true; 
        if (!waterSmsSent) {
            if(sendSMS("ALERT: Water Critical! Level: " + String(currentLiters, 1) + "L")) {
                waterSmsSent = true;
            }
            logToSD("WATER_LOW", String(currentLiters));
            updateFirebase("WATER_LOW_EVENT");
        }
    }
    
    if (wasLow && currentLiters > 400.0) {
        tankFillCount++;
        wasLow = false; waterSmsSent = false; 
        logToSD("TANK_FILLED", String(tankFillCount));
        updateFirebase("TANK_REFILLED");
    }

    gasValue = analogRead(MQ2_PIN); 
    temp = dht.readTemperature();   
    hum = dht.readHumidity();      
    motion = digitalRead(PIR_PIN); 

    // --- 2. PEOPLE COUNTER ---
    static int lastOuter = 4095, lastInner = 4095;
    int curOuter = analogRead(LDR_OUTER);
    int curInner = analogRead(LDR_INNER);

    if (curOuter < 1500 && lastOuter >= 1500) { 
        totalUsageCount++; peopleInCount++;
        logToSD("ENTRY", String(totalUsageCount));
        updateFirebase("ENTRY");
        delay(500);
    }
    if (curInner < 1500 && lastInner >= 1500) { 
        if (peopleInCount > 0) peopleInCount--;
        updateFirebase("EXIT");
        delay(500);
    }
    lastOuter = curOuter; lastInner = curInner;

    // --- 3. ALERTS & LED LOGIC ---
    if (totalUsageCount >= USAGE_SMS_LIMIT || gasValue > 2500) {
        digitalWrite(LED_RED, HIGH);
        digitalWrite(LED_GREEN, LOW); // Danger - Green OFF
        digitalWrite(BUZZER, HIGH);
        if (!usageSmsSent) {
            if(sendSMS("ALERT: Cleaning Required! Usage:" + String(totalUsageCount))) {
                usageSmsSent = true;
            }
            updateFirebase("CLEAN_ALERT_SMS");
        }
    } else {
        digitalWrite(LED_RED, LOW);
        digitalWrite(LED_GREEN, HIGH); // Safe - Green ON
        digitalWrite(BUZZER, LOW);
    }

    // --- 4. RESET BUTTON ---
    if (digitalRead(BTN_RESET) == LOW) {
        cleanCount++; peopleInCount = 0; totalUsageCount = 0; usageSmsSent = false;
        logToSD("CLEANED", "Count:" + String(cleanCount));
        updateFirebase("CLEANED");
        lcd.clear(); lcd.print("CLEAN RECORDED"); 
        Serial.println("System Reset: Clean Cycle Recorded");
        delay(1000);
    }

    // --- 5. DETAILED SERIAL MONITORING ---
    if (millis() - lastSerialPrint > SERIAL_PRINT_INTERVAL) {
        Serial.println("---------------------------");
        Serial.printf("Occupancy: %d | Total Usage: %d\n", peopleInCount, totalUsageCount);
        Serial.printf("Water Level: %.1f L | Gas: %d\n", currentLiters, gasValue);
        Serial.printf("Temp: %.1fC | Hum: %.1f%% | SMS: %s\n", temp, hum, lastSmsStatus.c_str());
        Serial.printf("Refills: %d | Cleanings: %d\n", tankFillCount, cleanCount);
        lastSerialPrint = millis();
    }

    if (millis() - lastFirebaseLog > FIREBASE_UPDATE_INTERVAL) {
        updateFirebase("AUTO_SYNC");
        lastFirebaseLog = millis();
    }

    // --- 6. LCD INTERFACE ---
    if (digitalRead(BTN_DISPLAY) == LOW) { 
        displayActive = true; displayStartTime = millis();
        lcd.backlight();
    }
    if (displayActive) {
        if (millis() - displayStartTime > 20000) {
            displayActive = false; lcd.noBacklight(); lcd.clear();
        } else {
            int toggle = (millis() / 3000) % 4;
            lcd.setCursor(0,0);
            if (toggle == 0) {
                lcd.print("In:"); lcd.print(peopleInCount); lcd.print(" Clean:"); lcd.print(cleanCount);
                lcd.setCursor(0,1); lcd.print("Total Use:"); lcd.print(totalUsageCount);
            } else if (toggle == 1) {
                lcd.print("Water: "); lcd.print((int)currentLiters); lcd.print(" L");
                lcd.setCursor(0,1); lcd.print("Refills: "); lcd.print(tankFillCount);
            } else if (toggle == 2) {
                lcd.print("Last SMS:");
                lcd.setCursor(0,1); lcd.print(lastSmsStatus);
            } else {
                lcd.print("Gas:"); lcd.print(gasValue); lcd.print(" T:"); lcd.print(temp,0);
                lcd.setCursor(0,1); lcd.print("M:"); lcd.print(motion?"YES":"NO"); lcd.print(" H:"); lcd.print(hum,0);
            }
        }
    }
    delay(50);
}