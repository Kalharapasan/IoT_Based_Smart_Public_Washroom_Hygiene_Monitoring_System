#include <esp_now.h>
#include <WiFi.h>


uint8_t broadcastAddress[] = {0xD8, 0x13, 0x2A, 0x73, 0x7B, 0x10}; 


const int trigPin = 5;
const int echoPin = 18;

typedef struct struct_message {
    float distance;
    int percentage; 
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}
// -------------------------------------------------------

void setup() {
  Serial.begin(115200);
  
  pinMode(trigPin, OUTPUT); 
  pinMode(echoPin, INPUT);
  
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb((esp_now_send_cb_t)OnDataSent);
  
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
}

void loop() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH);
  float distanceCm = duration * 0.034 / 2;

  if (distanceCm > 0) {
    myData.distance = distanceCm;
    myData.percentage = 0; 

    esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
    
    Serial.print("Distance Sent: ");
    Serial.println(distanceCm);
  }

  delay(2000); 
}