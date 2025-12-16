#include <Arduino.h>
#include <WiFi.h>

void setup()
{
    Serial.begin(115200);

    // Wait for Serial to stabilize
    delay(2000);
    Serial.println("\n\n--- ESP32 MAC ADDRESS FINDER ---");

    // Init WiFi in Station Mode to get the correct STA MAC
    WiFi.mode(WIFI_STA);

    Serial.print("Board Type: Sunton ESP32-S3 Screen\n");
    Serial.print("MAC Address: ");
    Serial.println(WiFi.macAddress());

    Serial.println("--------------------------------\n");
    Serial.println("Copy the MAC above and save it!");
}

void loop()
{
    // Print every 5 seconds just in case you missed it
    delay(5000);
    Serial.print("MAC Address: ");
    Serial.println(WiFi.macAddress());
}