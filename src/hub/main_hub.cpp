#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <WiFiManager.h>
#include <esp_now.h>

// IMPORT SECRETS
#include "secrets.h"

// --- PIN CONFIGURATION ---
#define PIN_SDA 11
#define PIN_SCL 12
#define PIN_RAIN_DIGITAL 10
#define PIN_FERT_LEVEL 14
// #define PIN_PUMP 4  // <--- RELAY DISABLED FOR NOW

// --- OBJECTS ---
Adafruit_BME280 bme;
BH1750 lightMeter;
WiFiManager wm;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// --- ESP-NOW TARGET ADDRESS (The Sunton Screen) ---
// MAC: 30:ED:A0:36:28:9C
uint8_t screenAddress[] = {0x30, 0xED, 0xA0, 0x36, 0x28, 0x9C};

// --- DATA PACKET FOR SCREEN ---
typedef struct struct_sensor_data
{
    float temp;
    float humid;
    float pressure;
    float lux;
    bool rain; // 0 = Dry, 1 = Rain
    bool fert; // 0 = Low, 1 = Full
    bool pump; // 0 = Off, 1 = On
} struct_sensor_data;

struct_sensor_data sensorData;

// --- TIMERS ---
unsigned long lastSensorRead = 0;
unsigned long lastDbRead = 0;
unsigned long lastEspNowSend = 0;
unsigned long ignoreDbUntil = 0;

void setup()
{
    Serial.begin(115200);

    // 1. PIN INIT
    pinMode(PIN_RAIN_DIGITAL, INPUT_PULLUP);
    pinMode(PIN_FERT_LEVEL, INPUT_PULLUP);

    // --- RELAY SETUP DISABLED ---
    // pinMode(PIN_PUMP, OUTPUT);
    // digitalWrite(PIN_PUMP, HIGH);

    // 2. SENSORS
    Wire.begin(PIN_SDA, PIN_SCL);
    if (!bme.begin(0x76))
        Serial.println("Warning: BME280 not found");
    if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE))
        Serial.println("Warning: BH1750 not found");

    // 3. WIFI (Connects to Router)
    // ESP-NOW requires the device to be on the SAME Channel as the receiver
    bool res = wm.autoConnect("GreenGuardian_Setup");
    if (!res)
        ESP.restart();

    // 4. INIT ESP-NOW
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Error initializing ESP-NOW");
    }

    // Register Peer (The Screen)
    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, screenAddress, 6);
    peerInfo.channel = 0; // 0 = use current WiFi channel
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        Serial.println("Failed to add peer");
    }
    else
    {
        Serial.println("Screen Paired Successfully!");
    }

    // 5. FIREBASE
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    config.signer.tokens.legacy_token = DATABASE_SECRET;
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
}

void loop()
{
    // --- 1. SENSOR READING (Fast Update for Screen) ---
    if (millis() - lastSensorRead > 500)
    {
        lastSensorRead = millis();

        sensorData.temp = bme.readTemperature();
        sensorData.humid = bme.readHumidity();
        sensorData.pressure = bme.readPressure() / 100.0F;
        sensorData.lux = lightMeter.readLightLevel();

        // Logic: LOW (0) on pin means Active (Wet/Full/On)
        sensorData.rain = (digitalRead(PIN_RAIN_DIGITAL) == LOW);
        sensorData.fert = (digitalRead(PIN_FERT_LEVEL) == LOW);

        // Default Pump to OFF since relay is disabled
        sensorData.pump = false;
    }

    // --- 2. ESP-NOW SEND (Every 500ms) ---
    if (millis() - lastEspNowSend > 500)
    {
        lastEspNowSend = millis();
        esp_err_t result = esp_now_send(screenAddress, (uint8_t *)&sensorData, sizeof(sensorData));

        if (result == ESP_OK)
        {
            // Serial.println("Sent to Screen"); // Uncomment to debug
        }
        else
        {
            Serial.println("Send Fail");
        }
    }

    // --- 3. FIREBASE UPLOAD (Every 5 Seconds) ---
    static unsigned long lastFirebaseUpload = 0;
    if (millis() - lastFirebaseUpload > 5000)
    {
        lastFirebaseUpload = millis();
        if (Firebase.ready())
        {
            FirebaseJson json;
            json.set("temperature", sensorData.temp);
            json.set("humidity", sensorData.humid);
            json.set("pressure", sensorData.pressure);
            json.set("lux", sensorData.lux);
            json.set("rain_detected", sensorData.rain);
            json.set("fertilizer_full", sensorData.fert);
            json.set("water_pump_is_on", sensorData.pump);
            Firebase.RTDB.setJSON(&fbdo, "/MCU", &json);
        }
    }
}