#include "WiFi.h"

void setup(){
  Serial.begin(9600);
  WiFi.mode(WIFI_MODE_STA);
  Serial.print("Receiver MAC Address: ");
  Serial.println(WiFi.macAddress());
}
 
void loop(){

  Serial.print("Receiver MAC Address: ");
  Serial.println(WiFi.macAddress());
  delay(1000);

}