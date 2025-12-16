#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <WiFiManager.h>

// IMPORT SECRETS
#include "secrets.h"

// --- PIN CONFIGURATION ---
#define PIN_SDA 11
#define PIN_SCL 12
#define PIN_RAIN_DIGITAL 10
#define PIN_FERT_LEVEL 14
#define PIN_PUMP 4

// --- OBJECTS ---
Adafruit_BME280 bme;
BH1750 lightMeter;
WiFiManager wm;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// --- TIMERS ---
unsigned long lastSensorRead = 0;
unsigned long lastDbRead = 0;

// NEW: Timer to prevent DB from overwriting manual toggle
unsigned long ignoreDbUntil = 0;

void setup()
{
    Serial.begin(115200);

    // 1. PIN INIT
    // Rain: Fake "Wet" by connecting D10 to GND
    pinMode(PIN_RAIN_DIGITAL, INPUT_PULLUP);

    // Fert: Float Switch (Pin 14 to GND)
    pinMode(PIN_FERT_LEVEL, INPUT_PULLUP);

    // Pump: Active LOW Relay (LOW = ON)
    pinMode(PIN_PUMP, OUTPUT);
    digitalWrite(PIN_PUMP, LOW); // Default OFF

    // 2. SENSORS INIT
    Wire.begin(PIN_SDA, PIN_SCL);
    if (!bme.begin(0x76))
        Serial.println("Warning: BME280 not found");
    if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE))
        Serial.println("Warning: BH1750 not found");

    // 3. WIFI MANAGER
    bool res = wm.autoConnect("GreenGuardian_Setup");
    if (!res)
    {
        Serial.println("Failed to connect");
        ESP.restart();
    }

    // 4. FIREBASE INIT
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    config.signer.tokens.legacy_token = DATABASE_SECRET;
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
}

void loop()
{
    // --- 1. MANUAL TOGGLE (FIXED) ---
    if (Serial.available())
    {
        char c = Serial.read();
        if (c == 't' || c == 'T')
        {
            // 1. Flip Physical State
            int current = digitalRead(PIN_PUMP);
            int newState = !current;
            digitalWrite(PIN_PUMP, newState);

            Serial.print("\n[MANUAL] Toggled Pump to: ");
            Serial.println(newState == LOW ? "ON" : "OFF");

            // 2. Tell Firebase the new state
            if (Firebase.ready())
            {
                bool dbState = (newState == LOW); // LOW = ON
                Firebase.RTDB.setBool(&fbdo, "/MCU/sensors/water_pump_is_on", dbState);
            }

            // 3. CRITICAL FIX: Ignore Database "Read" for 3 seconds
            // This prevents the DB from instantly turning it back off
            ignoreDbUntil = millis() + 3000;
        }
    }

    // --- 2. READ PUMP COMMAND FROM DB ---
    // Only run if we are NOT currently ignoring the DB
    if (millis() > ignoreDbUntil)
    {
        if (Firebase.ready() && (millis() - lastDbRead > 1000))
        {
            lastDbRead = millis();

            if (Firebase.RTDB.getBool(&fbdo, "/MCU/sensors/water_pump_is_on"))
            {
                bool shouldBeOn = fbdo.boolData();
                int targetPinState = shouldBeOn ? LOW : HIGH;

                // Only write if different (prevents glitching)
                if (digitalRead(PIN_PUMP) != targetPinState)
                {
                    digitalWrite(PIN_PUMP, targetPinState);
                    Serial.print("[CLOUD] Pump Updated: ");
                    Serial.println(shouldBeOn ? "ON" : "OFF");
                }
            }
        }
    }

    // --- 3. SENSOR READ & UPLOAD (Every 2 Seconds) ---
    if (millis() - lastSensorRead > 2000)
    {
        lastSensorRead = millis();

        // Read Sensors
        float t = bme.readTemperature();
        float h = bme.readHumidity();
        float p = bme.readPressure() / 100.0F;
        float l = lightMeter.readLightLevel();

        // Logic: LOW (0) means Connected to GND (Wet/Full)
        bool isRainWet = (digitalRead(PIN_RAIN_DIGITAL) == LOW);
        bool isFertFull = (digitalRead(PIN_FERT_LEVEL) == LOW);
        bool isPumpOn = (digitalRead(PIN_PUMP) == LOW);

        // Print Status
        Serial.println("\n--- STATUS ---");
        Serial.print("Rain (D10): ");
        Serial.println(isRainWet ? "WET (GND Connected)" : "DRY (Open)");
        Serial.print("Pump (D4):  ");
        Serial.println(isPumpOn ? "ON (Low)" : "OFF (High)");

        // Upload
        if (Firebase.ready())
        {
            FirebaseJson json;
            json.set("temperature", t);
            json.set("humidity", h);
            json.set("pressure", p);
            json.set("lux", l);
            json.set("rain_detected", isRainWet);
            json.set("fertilizer_full", isFertFull);
            json.set("water_pump_is_on", isPumpOn);

            Firebase.RTDB.setJSON(&fbdo, "/MCU/sensors", &json);
        }
    }
}