#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// --- CONFIGURATION ---
// TARGET: Waveshare S3 Nano (Hub)
// MAC: A0:85:E3:E1:2E:70
uint8_t hubMacAddress[] = {0xA0, 0x85, 0xE3, 0xE1, 0x2E, 0x70};

// Must match the structure in the Hub code
typedef struct struct_message
{
    char command[32];
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

// Callback to see if Hub received the packet
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setup()
{
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    esp_now_register_send_cb(OnDataSent);

    // Register the Hub as a peer
    memcpy(peerInfo.peer_addr, hubMacAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        Serial.println("Failed to add peer");
        return;
    }

    Serial.println("--- REMOTE READY ---");
    Serial.println("Type 't' in Serial Monitor to toggle pump.");
}

void loop()
{
    if (Serial.available())
    {
        String input = Serial.readStringUntil('\n');
        input.trim();

        if (input.equalsIgnoreCase("t") || input.equalsIgnoreCase("toggle"))
        {
            strcpy(myData.command, "TOGGLE_PUMP");
            Serial.println("Sending: TOGGLE_PUMP");
        }
        else if (input.equalsIgnoreCase("on"))
        {
            strcpy(myData.command, "PUMP_ON");
            Serial.println("Sending: PUMP_ON");
        }
        else if (input.equalsIgnoreCase("off"))
        {
            strcpy(myData.command, "PUMP_OFF");
            Serial.println("Sending: PUMP_OFF");
        }
        else
        {
            return;
        }

        esp_err_t result = esp_now_send(hubMacAddress, (uint8_t *)&myData, sizeof(myData));

        if (result != ESP_OK)
        {
            Serial.println("Error sending data");
        }
    }
}