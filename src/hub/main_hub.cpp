#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <esp_now.h>
#include <WiFi.h>

// --- PIN CONFIGURATION ---
#define PIN_SDA 11
#define PIN_SCL 12
#define PIN_RAIN_DIGITAL 10
#define PIN_FERT_LEVEL 14

// !!! MOVED TO GPIO 4 (Safer than GPIO 0) !!!
#define PIN_PUMP_RELAY 4

#define PIN_TX_TO_SCREEN 44
#define PIN_RX_FROM_SCREEN 43

// --- SECURITY: AUTHORIZED REMOTE ---
const uint8_t remoteMac[] = {0x58, 0xBF, 0x25, 0x12, 0xD6, 0x88};

Adafruit_BME280 bme;
BH1750 lightMeter;
HardwareSerial ScreenSerial(1);

typedef struct struct_message
{
    char command[32];
} struct_message;

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    if (memcmp(mac, remoteMac, 6) != 0)
        return;

    struct_message *msg = (struct_message *)incomingData;
    Serial.print("Command: ");
    Serial.println(msg->command);

    // --- LOGIC FIXED FOR ACTIVE LOW RELAY ---
    // ON  = LOW
    // OFF = HIGH

    if (strcmp(msg->command, "TOGGLE_PUMP") == 0)
    {
        int state = digitalRead(PIN_PUMP_RELAY);
        digitalWrite(PIN_PUMP_RELAY, !state); // Flip state
        // Log the human-readable status
        Serial.println(!state == LOW ? "Action: Pump ON" : "Action: Pump OFF");
    }
    else if (strcmp(msg->command, "PUMP_ON") == 0)
    {
        digitalWrite(PIN_PUMP_RELAY, LOW); // LOW IS ON
        Serial.println("Action: Pump ON");
    }
    else if (strcmp(msg->command, "PUMP_OFF") == 0)
    {
        digitalWrite(PIN_PUMP_RELAY, HIGH); // HIGH IS OFF
        Serial.println("Action: Pump OFF");
    }
}

void setup()
{
    Serial.begin(115200);
    ScreenSerial.begin(115200, SERIAL_8N1, PIN_RX_FROM_SCREEN, PIN_TX_TO_SCREEN);

    pinMode(PIN_RAIN_DIGITAL, INPUT);
    pinMode(PIN_FERT_LEVEL, INPUT_PULLUP);

    pinMode(PIN_PUMP_RELAY, OUTPUT);
    // START HIGH so the pump stays OFF when booting
    digitalWrite(PIN_PUMP_RELAY, HIGH);

    Wire.begin(PIN_SDA, PIN_SCL);
    if (!bme.begin(0x76))
        Serial.println("Warning: BME280 not found");
    if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE))
        Serial.println("Warning: BH1750 not found");

    WiFi.mode(WIFI_STA);
    if (esp_now_init() == ESP_OK)
    {
        esp_now_register_recv_cb(OnDataRecv);
        Serial.println("Hub Ready.");
    }
}

void loop()
{
    float t = bme.readTemperature();
    float h = bme.readHumidity();
    float p = bme.readPressure() / 100.0F;
    float l = lightMeter.readLightLevel();
    int rain = digitalRead(PIN_RAIN_DIGITAL);
    int fert = digitalRead(PIN_FERT_LEVEL);

    String packet = "T=" + String(t, 1) +
                    ";H=" + String(h, 0) +
                    ";P=" + String(p, 0) +
                    ";L=" + String(l, 0) +
                    ";R=" + String(rain) +
                    ";F=" + String(fert) + ";\n";

    ScreenSerial.print(packet);
    delay(1000);
}