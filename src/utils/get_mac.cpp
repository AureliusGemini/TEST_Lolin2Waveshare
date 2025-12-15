// File: src/utils/get_mac.cpp
#include <Arduino.h>
#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n==================================");
  Serial.print("Board MAC Address:  ");
  Serial.println(WiFi.macAddress());
  Serial.println("==================================\n");
}

void loop() {
  // Do nothing, just sit here so you can read the serial monitor
  delay(1000);
}