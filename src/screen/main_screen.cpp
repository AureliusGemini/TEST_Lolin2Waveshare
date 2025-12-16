#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <lvgl.h>
#include <esp_now.h>
#include <WiFi.h>
#include "secrets.h" // Re-use secrets for WiFi credentials

// --- HARDWARE CONFIG (Sunton ESP32-4827S043R) ---
#define TFT_BL 2
Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
    40, 41, 39, 42, 45, 48, 47, 21, 14, 5, 6, 7, 15, 16, 4, 8, 3, 46, 9, 1,
    0, 8, 4, 43, 0, 8, 4, 12, 1, 16000000);
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(480, 272, bus, 0, true);
SPIClass mySpi = SPIClass(SPI);
XPT2046_Touchscreen ts(38, 18);

// --- DATA STRUCTURE (Must Match Hub) ---
typedef struct struct_sensor_data
{
    float temp;
    float humid;
    float pressure;
    float lux;
    bool rain;
    bool fert;
    bool pump;
} struct_sensor_data;
struct_sensor_data incomingData;

// --- UI WIDGETS ---
lv_obj_t *chart;
lv_chart_series_t *ser_temp;
lv_chart_series_t *ser_humid;
lv_chart_series_t *ser_lux;

// Labels for Grid
lv_obj_t *label_A1_val; // Temp
lv_obj_t *label_A2_val; // Humid
lv_obj_t *label_A3_val; // Pressure
lv_obj_t *label_B1_val; // Lux
lv_obj_t *label_B2_val; // Rain
lv_obj_t *label_B3_val; // Fert

// --- ESP-NOW CALLBACK ---
bool newData = false;
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingDataPtr, int len)
{
    memcpy(&incomingData, incomingDataPtr, sizeof(incomingData));
    newData = true;
}

// --- UI BUILDER HELPER ---
lv_obj_t *create_sensor_box(lv_obj_t *parent, int col, int row, const char *title, lv_color_t color, lv_obj_t **val_label)
{
    // Box dimensions
    int w = 100;
    int h = 60;
    int x_start = 10;
    int y_start = 135; // Start below the chart

    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x_start + (col * (w + 10)), y_start + (row * (h + 10)));
    lv_obj_set_style_bg_color(btn, color, 0);

    // Title
    lv_obj_t *lbl_title = lv_label_create(btn);
    lv_label_set_text(lbl_title, title);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 0);

    // Value
    *val_label = lv_label_create(btn);
    lv_label_set_text(*val_label, "--");
    lv_obj_align(*val_label, LV_ALIGN_CENTER, 0, 10);

    return btn;
}

void build_ui()
{
    // 1. CHART (Top Half)
    chart = lv_chart_create(lv_scr_act());
    lv_obj_set_size(chart, 340, 120);
    lv_obj_set_pos(chart, 10, 10);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, 20); // Show last 20 readings

    // Add Series
    ser_temp = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    ser_humid = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
    ser_lux = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_YELLOW), LV_CHART_AXIS_SECONDARY_Y);

    // 2. GRID SENSORS (Bottom Half)
    // Row A (A1, A2, A3)
    create_sensor_box(lv_scr_act(), 0, 0, "Temp (C)", lv_palette_main(LV_PALETTE_RED), &label_A1_val);
    create_sensor_box(lv_scr_act(), 1, 0, "Humid (%)", lv_palette_main(LV_PALETTE_BLUE), &label_A2_val);
    create_sensor_box(lv_scr_act(), 2, 0, "Press (hPa)", lv_palette_main(LV_PALETTE_GREY), &label_A3_val);

    // Row B (B1, B2, B3)
    create_sensor_box(lv_scr_act(), 0, 1, "Lux", lv_palette_main(LV_PALETTE_YELLOW), &label_B1_val);
    create_sensor_box(lv_scr_act(), 1, 1, "Rain", lv_palette_main(LV_PALETTE_CYAN), &label_B2_val);
    create_sensor_box(lv_scr_act(), 2, 1, "Fert Lvl", lv_palette_main(LV_PALETTE_GREEN), &label_B3_val);

    // 3. MENU SIDEBAR (Right Side)
    lv_obj_t *menu = lv_obj_create(lv_scr_act());
    lv_obj_set_size(menu, 110, 260);
    lv_obj_set_pos(menu, 360, 5);
    lv_obj_set_style_bg_color(menu, lv_palette_darken(LV_PALETTE_GREY, 3), 0);

    const char *menu_items[] = {"Settings", "Overview", "Zone 1"};
    for (int i = 0; i < 3; i++)
    {
        lv_obj_t *btn = lv_btn_create(menu);
        lv_obj_set_size(btn, 90, 50);
        lv_obj_set_pos(btn, 0, i * 60);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, menu_items[i]);
        lv_obj_center(lbl);
    }
}

// --- DISPLAY FLUSH ---
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf;
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
    lv_disp_flush_ready(disp);
}

void setup()
{
    Serial.begin(115200);

    // Init Display
    gfx->begin();
    gfx->fillScreen(BLACK);
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    // Init LVGL
    lv_init();
    disp_draw_buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * 480 * 30, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, 480 * 30);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 480;
    disp_drv.ver_res = 272;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    build_ui();

    // Init WiFi & ESP-NOW
    WiFi.mode(WIFI_STA);
    // Connect to same WiFi as Hub to sync channels
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000)
    {
        // Try for 10s then proceed anyway (channel might be wrong though)
        delay(100);
    }

    if (esp_now_init() == ESP_OK)
    {
        esp_now_register_recv_cb(OnDataRecv);
    }
}

void loop()
{
    lv_timer_handler();

    if (newData)
    {
        newData = false;

        // Update Labels
        lv_label_set_text_fmt(label_A1_val, "%.1f", incomingData.temp);
        lv_label_set_text_fmt(label_A2_val, "%.0f", incomingData.humid);
        lv_label_set_text_fmt(label_A3_val, "%.0f", incomingData.pressure);
        lv_label_set_text_fmt(label_B1_val, "%.0f", incomingData.lux);

        lv_label_set_text(label_B2_val, incomingData.rain ? "Rain" : "Dry");
        lv_label_set_text(label_B3_val, incomingData.fert ? "Full" : "Low");

        // Update Chart
        lv_chart_set_next_value(chart, ser_temp, (int)incomingData.temp);
        lv_chart_set_next_value(chart, ser_humid, (int)incomingData.humid);
        // Scale Lux to fit chart (0-100 range roughly)
        lv_chart_set_next_value(chart, ser_lux, (int)(incomingData.lux / 10.0));
    }
    delay(5);
}