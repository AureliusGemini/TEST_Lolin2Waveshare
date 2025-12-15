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
#define PIN_PUMP_RELAY 47
#define PIN_TX_TO_SCREEN 44
#define PIN_RX_FROM_SCREEN 43

// --- AUTHORIZED REMOTE (Lolin32 Lite) ---
// MAC: 58:BF:25:12:D6:88
const uint8_t remoteMac[] = {0x58, 0xBF, 0x25, 0x12, 0xD6, 0x88};

// --- OBJECTS ---
Adafruit_BME280 bme;
BH1750 lightMeter;
HardwareSerial ScreenSerial(1);

// --- ESP-NOW CALLBACK ---
typedef struct struct_message
{
    char command[32];
} struct_message;

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    // 1. SECURITY CHECK: Verify Sender MAC
    if (memcmp(mac, remoteMac, 6) != 0)
    {
        Serial.println("Ignored command from unauthorized device.");
        return;
    }

    struct_message *msg = (struct_message *)incomingData;
    Serial.print("Command Received: ");
    Serial.println(msg->command);

    // 2. EXECUTE COMMAND
    if (strcmp(msg->command, "TOGGLE_PUMP") == 0)
    {
        digitalWrite(PIN_PUMP_RELAY, !digitalRead(PIN_PUMP_RELAY));
        Serial.println("Action: Pump Toggled");
    }
    else if (strcmp(msg->command, "PUMP_ON") == 0)
    {
        digitalWrite(PIN_PUMP_RELAY, HIGH);
        Serial.println("Action: Pump ON");
    }
    else if (strcmp(msg->command, "PUMP_OFF") == 0)
    {
        digitalWrite(PIN_PUMP_RELAY, LOW);
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
    digitalWrite(PIN_PUMP_RELAY, LOW); // Start with Pump OFF

    Wire.begin(PIN_SDA, PIN_SCL);
    if (!bme.begin(0x76))
        Serial.println("Warning: BME280 not found");
    if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE))
        Serial.println("Warning: BH1750 not found");

    WiFi.mode(WIFI_STA);
    if (esp_now_init() == ESP_OK)
    {
        esp_now_register_recv_cb(OnDataRecv);
        Serial.println("Hub Ready. Waiting for Remote...");
    }
    else
    {
        Serial.println("ESP-NOW Init Failed");
    }
}

void loop()
{
    // 1. Read Sensors
    float t = bme.readTemperature();
    float h = bme.readHumidity();
    float p = bme.readPressure() / 100.0F;
    float l = lightMeter.readLightLevel();
    int rain = digitalRead(PIN_RAIN_DIGITAL);
    int fert = digitalRead(PIN_FERT_LEVEL);

    // 2. Format & Send to Screen
    // Packet: T=24.5;H=60;P=1001;L=500;R=1;F=0;
    String packet = "T=" + String(t, 1) +
                    ";H=" + String(h, 0) +
                    ";P=" + String(p, 0) +
                    ";L=" + String(l, 0) +
                    ";R=" + String(rain) +
                    ";F=" + String(fert) + ";\n";

    ScreenSerial.print(packet);
    // Serial.print("To Screen: "); Serial.println(packet); // Comment out to reduce Serial noise if debugging Remote

    delay(1000);
}