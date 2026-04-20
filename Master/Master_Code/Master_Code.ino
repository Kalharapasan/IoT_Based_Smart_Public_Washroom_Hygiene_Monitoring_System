#include <esp_now.h>       // For ESP-NOW communication (Water level data)
#include <WiFi.h>          // For WiFi connectivity
#include <Wire.h>          // For I2C communication (LCD)
#include <LiquidCrystal_I2C.h> // For LCD display control
#include <DHT.h>           // For Humidity and Temperature sensor
#include <SPI.h>           // For SPI communication (SD Card)
#include <SD.h>            // For SD Card storage
#include <FirebaseESP32.h> // For Firebase Realtime Database

// --- 1. NETWORK SETTINGS ---
#define WIFI_SSID      "Kalara-WiFi"                                          // Your WiFi Name
#define WIFI_PASSWORD  "Pasa2001#"                                            // Your WiFi Password
#define FIREBASE_HOST  "smart-washroom-system-default-rtdb.firebaseio.com"    // Firebase Host URL
#define FIREBASE_AUTH  "AIzaSyDn8CicqXCbBxy6jUyYJz_0f-sTu18XJ9E"             // Firebase Database Secret

// --- 2. UPDATE INTERVALS ---
unsigned long FIREBASE_UPDATE_INTERVAL = 1000; // Sync with Firebase every 5 seconds
unsigned long SERIAL_PRINT_INTERVAL = 2000;    // Print to Serial monitor every 2 seconds

// --- 3. PIN DEFINITIONS ---
#define SD_CS       5      // SD Card Chip Select
#define GSM_RX      16     // GSM RX Pin
#define GSM_TX      17     // GSM TX Pin
#define LDR_OUTER   34     // Entry LDR Sensor
#define LDR_INNER   35     // Exit LDR Sensor
#define MQ2_PIN     32     // Gas/Smoke Sensor
#define DHTPIN      25     // DHT22 Sensor Pin
#define PIR_PIN     26     // Motion Sensor Pin
#define BTN_DISPLAY 4      // Button to turn on LCD backlight
#define BTN_RESET   13     // Button to record cleaning (Reset)
#define LED_RED     12     // Red Alert LED
#define BUZZER      27     // Warning Buzzer

// --- 4. GLOBAL VARIABLES ---
float TANK_MAX_LITERS = 500.0; // Max water capacity is 500L
float WATER_LOW_LIMIT = 100.0; // Alert when water is below 100L
float TANK_H = 100.0;          // Height of the tank is 100cm

int peopleInCount = 0;      // Tracks people currently inside
int totalUsageCount = 0;    // Tracks usage since last clean
int cleanCount = 0;         // Tracks total cleaning sessions
int tankFillCount = 0;      // Tracks total tank refill cycles
int USAGE_SMS_LIMIT = 5;    // Max usage before cleaning alert
const String SMS_NUMBER = "+94723466524"; // Phone number for alerts

bool displayActive = false; // Flag to check if LCD is active
bool usageSmsSent = false;  // Prevents multiple cleaning SMS
bool waterSmsSent = false;  // Prevents multiple water level SMS
bool wasLow = false;        // Used to detect when tank is refilled
String lastSmsStatus = "N/A"; // Stores result of last SMS attempt

unsigned long displayStartTime = 0; // Timer for LCD auto-off
unsigned long lastFirebaseLog = 0;  // Timer for Cloud sync
unsigned long lastSerialPrint = 0;  // Timer for Serial logging

DHT dht(DHTPIN, DHT22);             // Initialize DHT object
LiquidCrystal_I2C lcd(0x27, 16, 2); // Initialize LCD object
HardwareSerial sim800(2);           // Initialize GSM Serial

FirebaseData firebaseData;          // Firebase data object
FirebaseAuth auth;                  // Firebase auth object
FirebaseConfig config;              // Firebase config object

float currentLiters = 0, hum = 0, temp = 0; // Sensor values
int gasValue = 0;                           // Gas level
bool motion = false;                        // Motion status

typedef struct struct_message { // Structure to receive distance
    float distance;
} struct_message;
struct_message myData;

// Callback function for when ESP-NOW data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    memcpy(&myData, incomingData, sizeof(myData));
}

// Function to log data to SD Card
void logToSD(String event, String details) {
    File dataFile = SD.open("/data_log.csv", FILE_APPEND); // Open file
    if (dataFile) {
        String data = String(millis()/1000) + "," + event + "," + details + ",SMS:" + lastSmsStatus;
        dataFile.println(data); // Write line
        dataFile.close();       // Close file
    }
}

// Function to send SMS and check if successful
bool sendSMS(String msg) {
    sim800.println("AT+CMGF=1"); delay(500); // Set to text mode
    sim800.print("AT+CMGS=\""); sim800.print(SMS_NUMBER); sim800.println("\""); // Set number
    delay(500); 
    sim800.print(msg); delay(200); // Send message content
    sim800.write(26); // Send Ctrl+Z to finish
    
    unsigned long start = millis();
    bool success = false;
    while (millis() - start < 5000) { // Wait 5 seconds for OK response
        if (sim800.find("OK")) { success = true; break; }
    }
    lastSmsStatus = success ? "SENT_OK" : "FAILED"; // Store result
    return success;
}

// Function to update Firebase Realtime Database
void updateFirebase(String event) {
    if (WiFi.status() == WL_CONNECTED) { // Only if WiFi is connected
        FirebaseJson json;
        json.set("Event", event);
        json.set("P_In", peopleInCount);
        json.set("Water_L", currentLiters); 
        json.set("SMS_Stat", lastSmsStatus);
        Firebase.updateNode(firebaseData, "/Live_Status", json); // Update current status
        Firebase.pushJSON(firebaseData, "/History", json);       // Log to history
    }
}

// --- MAIN SETUP ---
void setup() {
    Serial.begin(9600); // Start serial communication
    sim800.begin(9600, SERIAL_8N1, GSM_RX, GSM_TX); // Start GSM communication
    
    lcd.init();        // Initialize LCD
    lcd.backlight();   // Turn on backlight (STAYS ON during testing)
    lcd.clear();
    lcd.print("SYSTEM BOOTING"); // Display start message
    delay(1000);

    // 1. TESTING SENSORS & SD CARD
    lcd.clear(); lcd.print("TESTING SENSORS");
    dht.begin(); // Start DHT sensor
    pinMode(LDR_OUTER, INPUT); pinMode(LDR_INNER, INPUT); // Set LDR pins
    pinMode(PIR_PIN, INPUT); pinMode(MQ2_PIN, INPUT);     // Set sensor pins
    pinMode(BTN_DISPLAY, INPUT_PULLUP); pinMode(BTN_RESET, INPUT_PULLUP); // Set button pins
    pinMode(LED_RED, OUTPUT); pinMode(BUZZER, OUTPUT);    // Set alert pins
    
    if (SD.begin(SD_CS)) { // Start SD Card
        lcd.setCursor(0,1); lcd.print("SD CARD: OK");
    } else {
        lcd.setCursor(0,1); lcd.print("SD CARD: FAIL");
    }
    delay(1500);

    // 2. TESTING WIFI CONNECTION
    lcd.clear(); lcd.print("WiFi Connecting");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // Connect to WiFi
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20) { // Try for 10 seconds
        delay(500); lcd.print("."); retry++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        lcd.setCursor(0,1); lcd.print("WiFi: CONNECTED");
    } else {
        lcd.setCursor(0,1); lcd.print("WiFi: TIMEOUT");
    }
    delay(1500);

    // 3. TESTING FIREBASE CONNECTION
    lcd.clear(); lcd.print("FB CONNECTING...");
    config.host = FIREBASE_HOST;
    config.signer.tokens.legacy_token = FIREBASE_AUTH;
    Firebase.begin(&config, &auth); // Connect to Firebase
    Firebase.reconnectWiFi(true);
    
    if (Firebase.ready()) {
        lcd.setCursor(0,1); lcd.print("FIREBASE: READY");
    } else {
        lcd.setCursor(0,1); lcd.print("FIREBASE: ERROR");
    }
    delay(1500);

    // 4. ESP-NOW INITIALIZATION
    WiFi.mode(WIFI_AP_STA); // Set WiFi to Station mode
    if (esp_now_init() == ESP_OK) { // Start ESP-NOW
        esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv)); // Register receive callback
    }

    // FINAL STARTUP CONFIRMATION
    digitalWrite(BUZZER, HIGH); delay(300); digitalWrite(BUZZER, LOW); // Beep once
    lcd.clear(); lcd.print("ALL SYSTEMS OK"); // Success message
    lcd.setCursor(0,1); lcd.print("READY TO WORK");
    delay(3000); // Keep screen ON for 3 seconds to see results
    
    lcd.noBacklight(); // TEST FINISHED: Turn OFF backlight
    lcd.clear();       // Clear screen
}

void loop() {
    // --- 1. WATER LEVEL MONITORING ---
    float waterLevelCm = TANK_H - myData.distance; // Calculate water height
    if (waterLevelCm < 0) waterLevelCm = 0;
    currentLiters = (waterLevelCm / TANK_H) * TANK_MAX_LITERS; // Convert to liters

    // Check for Low Water (100L)
    if (currentLiters < WATER_LOW_LIMIT) {
        wasLow = true; 
        if (!waterSmsSent) { // Send SMS if not already sent
            if(sendSMS("ALERT: Water Critical! Only " + String(currentLiters, 1) + "L Left.")) {
                waterSmsSent = true;
            }
            logToSD("WATER_LOW", String(currentLiters));
            updateFirebase("WATER_LOW_EVENT");
        }
    }
    
    // Check if tank was refilled (400L+)
    if (wasLow && currentLiters > 400.0) {
        tankFillCount++; // Increment fill count
        wasLow = false;
        waterSmsSent = false; // Allow next alert
        logToSD("TANK_FILLED", String(tankFillCount));
        updateFirebase("TANK_REFILLED");
    }

    gasValue = analogRead(MQ2_PIN); // Read gas sensor
    temp = dht.readTemperature();   // Read temperature
    hum = dht.readHumidity();      // Read humidity
    motion = digitalRead(PIR_PIN); // Read motion sensor

    // --- 2. PEOPLE COUNTER (LDR) ---
    static int lastOuter = 4095, lastInner = 4095;
    int curOuter = analogRead(LDR_OUTER);
    int curInner = analogRead(LDR_INNER);

    if (curOuter < 1500 && lastOuter >= 1500) { // Person entering
        totalUsageCount++; peopleInCount++;
        logToSD("ENTRY", String(totalUsageCount));
        updateFirebase("ENTRY");
        delay(500);
    }
    if (curInner < 1500 && lastInner >= 1500) { // Person exiting
        if (peopleInCount > 0) peopleInCount--;
        updateFirebase("EXIT");
        delay(500);
    }
    lastOuter = curOuter; lastInner = curInner;

    // --- 3. MAINTENANCE ALERTS ---
    if (totalUsageCount >= USAGE_SMS_LIMIT || gasValue > 2500) {
        digitalWrite(LED_RED, HIGH); digitalWrite(BUZZER, HIGH); // Alarm ON
        if (!usageSmsSent) { // Send cleaning alert SMS
            if(sendSMS("ALERT: Cleaning Required! Usage:" + String(totalUsageCount))) {
                usageSmsSent = true;
            }
            updateFirebase("CLEAN_ALERT_SMS");
        }
    } else {
        digitalWrite(LED_RED, LOW); digitalWrite(BUZZER, LOW); // Alarm OFF
    }

    // --- 4. RESET BUTTON (CLEANED) ---
    if (digitalRead(BTN_RESET) == LOW) {
        cleanCount++;       // Increment clean cycle
        peopleInCount = 0;  // Reset counts
        totalUsageCount = 0; 
        usageSmsSent = false;
        logToSD("CLEANED", "Count:" + String(cleanCount));
        updateFirebase("CLEANED");
        lcd.clear(); lcd.print("CLEAN RECORDED"); 
        delay(1000);
    }

    // --- 5. LOGGING & SYNC ---
    if (millis() - lastSerialPrint > SERIAL_PRINT_INTERVAL) { // Periodically print status
        Serial.printf("[STATUS] P:%d | Use:%d | SMS:%s | Water:%.1fL\n", 
                      peopleInCount, totalUsageCount, lastSmsStatus.c_str(), currentLiters);
        lastSerialPrint = millis();
    }

    if (millis() - lastFirebaseLog > FIREBASE_UPDATE_INTERVAL) { // Sync with cloud
        updateFirebase("AUTO_SYNC");
        lastFirebaseLog = millis();
    }

    // --- 6. LCD INTERFACE (Wake-up on button press) ---
    if (digitalRead(BTN_DISPLAY) == LOW) { // Wake up display
        displayActive = true; displayStartTime = millis();
        lcd.backlight();
    }
    if (displayActive) {
        if (millis() - displayStartTime > 20000) { // Auto sleep after 20 seconds
            displayActive = false; lcd.noBacklight(); lcd.clear();
        } else {
            int toggle = (millis() / 3000) % 4; // Rotate screens every 3 seconds
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
    delay(50); // Short stability delay
}