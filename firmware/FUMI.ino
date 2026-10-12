/*
 * FUMI — Open-source AI companion for ESP32-S3
 *
 * Target hardware:
 *   Waveshare ESP32-S3 Touch LCD 1.46
 *
 * This firmware contains the FUMI display, touch, life simulation,
 * mini-game, Wi-Fi, AI chat, and push-to-talk voice integration.
 *
 * IMPORTANT:
 *   Copy fumi_config.h.example to fumi_config.h and fill in your
 *   own Wi-Fi and backend settings before compiling.
 *
 * License: MIT
 */

/*Using LVGL with Arduino requires some extra steps:
 *Be sure to read the docs here: https://docs.lvgl.io/master/get-started/platforms/arduino.html  */

#include "Display_SPD2010.h"
#include "Audio_PCM5101.h"
#include "RTC_PCF85063.h"
#include "esp_heap_caps.h"
#include "Gyro_QMI8658.h"
#include "LVGL_Driver.h"
#include "MIC_MSM.h"
#include "ESP_I2S.h"
#include "PWR_Key.h"
#include "SD_Card.h"
#include "LVGL_Example.h"
#include "BAT_Driver.h"
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "freertos/semphr.h"

String fumi_json_string(const String &json, const char *key);


void Driver_Loop(void *parameter)
{
    Serial.println("========== FUMI DRIVER LOOP 123456789 ==========");

    //Wireless_Test2();

    while(1)
  {
    PWR_Loop();
    QMI8658_Loop();
    PCF85063_Loop();
    BAT_Get_Volts();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
lv_obj_t *fumi_face = NULL;
lv_timer_t *fumi_blink_timer = NULL;
lv_obj_t *fumi_hunger_ring = NULL;
lv_obj_t *fumi_happiness_ring = NULL;
lv_timer_t *fumi_hunger_timer = NULL;
lv_timer_t *fumi_energy_timer = NULL;
lv_timer_t *fumi_happiness_timer = NULL;
lv_timer_t *fumi_idle_timer = NULL;
lv_obj_t *fumi_energy_bar = NULL;
lv_obj_t *fumi_happiness_bar = NULL;

WebServer fumi_server(80);

bool fumi_ap_mode = false;
bool fumi_wifi_configured = false;

bool fumi_blinking = false;
bool fumi_sleeping = false;

int fumi_expression = 0;

int fumi_hunger = 20;
int fumi_happiness = 80;
int fumi_energy = 90;

// FUMI personality
int fumi_affection = 0;
int fumi_playfulness = 50;
int fumi_bond = 0;
Preferences fumi_preferences;

lv_obj_t *fumi_food = NULL;

// =========================
// FUMI STATUS SCREEN
// =========================
lv_obj_t *fumi_status_screen = NULL;
bool fumi_status_open = false;
int32_t fumi_swipe_start_x = 0;
int32_t fumi_swipe_start_y = 0;

// Live status labels
lv_obj_t *fumi_status_bond_value = NULL;
lv_obj_t *fumi_status_happiness_value = NULL;
lv_obj_t *fumi_status_energy_value = NULL;
lv_obj_t *fumi_status_hunger_value = NULL;
lv_obj_t *fumi_status_growth_value = NULL;
lv_obj_t *fumi_status_day_value = NULL;

// Persistent age / growth
uint32_t fumi_age_seconds = 0;
lv_timer_t *fumi_age_timer = NULL;

// Interactive life / mini-game
bool fumi_game_active = false;
int fumi_game_score = 0;
int fumi_game_elapsed = 0;
int fumi_games_played = 0;
int fumi_games_won = 0;
lv_obj_t *fumi_game_target = NULL;
lv_obj_t *fumi_game_score_label = NULL;
lv_obj_t *fumi_game_title_label = NULL;
lv_timer_t *fumi_game_timer = NULL;

// =========================
// FUMI AI BRAIN
// =========================
#include "fumi_config.h"

const char *FUMI_WIFI_SSID = FUMI_WIFI_SSID_CONFIG;
const char *FUMI_WIFI_PASSWORD = FUMI_WIFI_PASSWORD_CONFIG;
const char *FUMI_BRAIN_URL = FUMI_BRAIN_URL_CONFIG;
const char *FUMI_VOICE_URL = FUMI_VOICE_URL_CONFIG;

bool fumi_ai_busy = false;

// =========================
// FUMI VOICE INPUT
// =========================

I2SClass fumi_mic;

bool fumi_mic_ready = false;
bool fumi_recording = false;
lv_obj_t *fumi_voice_button = NULL;
bool fumi_voice_pressed = false;
uint32_t fumi_voice_record_start = 0;
#define FUMI_MAX_RECORD_SECONDS 5
#define FUMI_RECORD_CHANNELS 2
int16_t *fumi_audio_buffer = NULL;
size_t fumi_audio_samples = 0;
size_t fumi_audio_capacity = 0;
uint8_t fumi_voice_tap_count = 0;
uint32_t fumi_voice_last_tap = 0;

#define FUMI_MIC_BCK 15
#define FUMI_MIC_WS  2
#define FUMI_MIC_DIN 39

#define FUMI_SAMPLE_RATE 16000
#define FUMI_RECORD_SECONDS 3

void fumi_mic_init()
{
    Serial.println(">>> FUMI MIC INIT");

    fumi_mic.setPins(
        FUMI_MIC_BCK,
        FUMI_MIC_WS,
        -1,
        FUMI_MIC_DIN
    );

    fumi_mic.setTimeout(1000);

    bool ok = fumi_mic.begin(
        I2S_MODE_STD,
        FUMI_SAMPLE_RATE,
        I2S_DATA_BIT_WIDTH_16BIT,
        I2S_SLOT_MODE_STEREO
    );

    if (!ok)
    {
        Serial.println(">>> FUMI MIC FAILED");
        fumi_mic_ready = false;
        return;
    }

    fumi_mic_ready = true;

    Serial.println(">>> FUMI MIC READY");
}

void fumi_mic_test()
{
  if (!fumi_mic_ready)
  {
    Serial.println(">>> MIC NOT READY");
    return;
  }

  Serial.println();
  Serial.println("================================");
  Serial.println(">>> FUMI MIC TEST START");
  Serial.println(">>> SPEAK NOW FOR 3 SECONDS!");
  Serial.println("================================");

  const int BUFFER_SAMPLES = 512;
  int16_t buffer[BUFFER_SAMPLES];

  int64_t sumSquares = 0;
  int32_t peak = 0;
  uint32_t totalSamples = 0;

  uint32_t startTime = millis();

  while (millis() - startTime < 3000)
  {
    size_t bytesRead = fumi_mic.readBytes(
      (char *)buffer,
      sizeof(buffer)
    );

    if (bytesRead == 0)
      continue;

    size_t samplesRead = bytesRead / sizeof(int16_t);

    for (size_t i = 0; i < samplesRead; i++)
    {
      int32_t sample = buffer[i];

      if (sample < 0)
        sample = -sample;

      if (sample > peak)
        peak = sample;

      sumSquares += (int64_t)sample * sample;
      totalSamples++;
    }
  }

  float rms = 0;

  if (totalSamples > 0)
  {
    rms = sqrt(
      (double)sumSquares / (double)totalSamples
    );
  }

  Serial.println();
  Serial.println("================================");
  Serial.println(">>> FUMI MIC TEST END");

  Serial.print(">>> SAMPLES: ");
  Serial.println(totalSamples);

  Serial.print(">>> PEAK: ");
  Serial.println(peak);

  Serial.print(">>> RMS: ");
  Serial.println(rms, 2);

  if (peak < 100)
  {
    Serial.println(">>> RESULT: VERY LOW / NO AUDIO");
  }
  else if (peak < 1000)
  {
    Serial.println(">>> RESULT: AUDIO DETECTED - LOW");
  }
  else
  {
    Serial.println(">>> RESULT: AUDIO DETECTED!");
  }

  Serial.println("================================");
}

lv_timer_t *fumi_ai_timer = NULL;
lv_obj_t *fumi_ai_label = NULL;
String fumi_ai_last_message = "";
volatile bool fumi_ai_response_ready = false;
String fumi_ai_pending_response = "";
String fumi_ai_pending_event = "";

// Voice response state (written by voice HTTP task, consumed by LVGL timer)
volatile bool fumi_voice_response_ready = false;
String fumi_voice_pending_reply = "";
String fumi_voice_pending_transcript = "";
int fumi_voice_pending_expression = 0;
bool fumi_voice_upload_busy = false;

SemaphoreHandle_t fumi_ai_mutex = NULL;
lv_timer_t *fumi_ai_apply_timer = NULL;

void fumi_start_recording()
{
    if (!fumi_mic_ready)
    {
        Serial.println(">>> MIC NOT READY");
        return;
    }

    if (fumi_voice_upload_busy)
    {
        Serial.println(">>> VOICE PROCESSING BUSY");
        return;
    }

    if (fumi_audio_buffer != NULL)
    {
        free(fumi_audio_buffer);
        fumi_audio_buffer = NULL;
    }

    fumi_audio_capacity =
        FUMI_SAMPLE_RATE *
        FUMI_MAX_RECORD_SECONDS *
        FUMI_RECORD_CHANNELS;

    Serial.println(">>> ALLOCATING AUDIO BUFFER...");

    fumi_audio_buffer = (int16_t *)ps_malloc(
        fumi_audio_capacity * sizeof(int16_t)
    );

    if (fumi_audio_buffer == NULL)
    {
        Serial.println(">>> AUDIO BUFFER ALLOCATION FAILED");
        fumi_audio_capacity = 0;
        return;
    }

    fumi_audio_samples = 0;
    fumi_recording = true;

    Serial.println(">>> AUDIO BUFFER READY");
    Serial.print(">>> CAPACITY SAMPLES: ");
    Serial.println(fumi_audio_capacity);

    Serial.print(">>> CAPACITY BYTES: ");
    Serial.println(
        fumi_audio_capacity * sizeof(int16_t)
    );
}

void fumi_recording_loop()
{
    if (!fumi_recording)
        return;

    if (fumi_audio_buffer == NULL)
        return;

    if (fumi_audio_samples >= fumi_audio_capacity)
    {
        Serial.println(">>> MAX RECORDING REACHED");
        fumi_recording = false;
        return;
    }

    const size_t CHUNK_SAMPLES = 512;

    size_t remaining =
        fumi_audio_capacity -
        fumi_audio_samples;

    size_t samplesToRead =
        min(CHUNK_SAMPLES, remaining);

    size_t bytesRead = fumi_mic.readBytes(
        (char *)(fumi_audio_buffer + fumi_audio_samples),
        samplesToRead * sizeof(int16_t)
    );

    if (bytesRead > 0)
    {
        fumi_audio_samples +=
            bytesRead / sizeof(int16_t);
    }
}

void fumi_write_wav_u32(uint8_t *p, uint32_t value)
{
    p[0] = value & 0xFF;
    p[1] = (value >> 8) & 0xFF;
    p[2] = (value >> 16) & 0xFF;
    p[3] = (value >> 24) & 0xFF;
}

void fumi_write_wav_u16(uint8_t *p, uint16_t value)
{
    p[0] = value & 0xFF;
    p[1] = (value >> 8) & 0xFF;
}

void fumi_prepare_wav(
    uint8_t *wav,
    size_t &wav_size
)
{
    // I2S is captured as stereo 16-bit samples.
    // Convert interleaved stereo -> mono by averaging L/R.
    size_t mono_samples =
        fumi_audio_samples / FUMI_RECORD_CHANNELS;

    uint32_t data_bytes =
        (uint32_t)(mono_samples * sizeof(int16_t));

    wav_size = 44 + data_bytes;

    // RIFF/WAVE header
    memcpy(wav + 0, "RIFF", 4);
    fumi_write_wav_u32(
        wav + 4,
        36 + data_bytes
    );

    memcpy(wav + 8, "WAVE", 4);
    memcpy(wav + 12, "fmt ", 4);

    fumi_write_wav_u32(wav + 16, 16); // PCM fmt chunk
    fumi_write_wav_u16(wav + 20, 1);  // PCM
    fumi_write_wav_u16(wav + 22, 1);  // mono
    fumi_write_wav_u32(
        wav + 24,
        FUMI_SAMPLE_RATE
    );

    uint32_t byte_rate =
        FUMI_SAMPLE_RATE *
        sizeof(int16_t);

    fumi_write_wav_u32(
        wav + 28,
        byte_rate
    );

    fumi_write_wav_u16(
        wav + 32,
        sizeof(int16_t)
    );

    fumi_write_wav_u16(
        wav + 34,
        16
    );

    memcpy(wav + 36, "data", 4);
    fumi_write_wav_u32(
        wav + 40,
        data_bytes
    );

    // Convert stereo samples into mono PCM.
    int16_t *out =
        (int16_t *)(wav + 44);

    for (size_t i = 0; i < mono_samples; i++)
    {
        int32_t left =
            fumi_audio_buffer[
                i * FUMI_RECORD_CHANNELS
            ];

        int32_t right =
            fumi_audio_buffer[
                i * FUMI_RECORD_CHANNELS + 1
            ];

        int32_t mono =
            (left + right) / 2;

        if (mono > 32767)
            mono = 32767;

        if (mono < -32768)
            mono = -32768;

        out[i] = (int16_t)mono;
    }
}

void fumi_voice_upload_task(void *parameter)
{
    (void)parameter;

    fumi_voice_upload_busy = true;

    Serial.println();
    Serial.println("================================");
    Serial.println(">>> FUMI VOICE UPLOAD START");

    if (
        WiFi.status() != WL_CONNECTED ||
        fumi_audio_buffer == NULL ||
        fumi_audio_samples < 2
    )
    {
        Serial.println(">>> VOICE UPLOAD ABORTED");
        fumi_voice_upload_busy = false;
        vTaskDelete(NULL);
        return;
    }

    size_t mono_samples =
        fumi_audio_samples / FUMI_RECORD_CHANNELS;

    size_t wav_size =
        44 + mono_samples * sizeof(int16_t);

    Serial.print(">>> WAV SIZE: ");
    Serial.println(wav_size);

    // Keep the original capture buffer untouched while creating WAV.
    uint8_t *wav_buffer =
        (uint8_t *)ps_malloc(wav_size);

    if (wav_buffer == NULL)
    {
        Serial.println(">>> WAV BUFFER ALLOCATION FAILED");
        fumi_voice_upload_busy = false;
        vTaskDelete(NULL);
        return;
    }

    fumi_prepare_wav(
        wav_buffer,
        wav_size
    );

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;

    Serial.println(">>> VOICE HTTP BEGIN");

    if (!http.begin(
        client,
        "fumi-brain-production.up.railway.app",
        443,
        "/voice",
        true
    ))
    {
        Serial.println(">>> VOICE HTTP BEGIN FAILED");
        free(wav_buffer);
        fumi_voice_upload_busy = false;
        vTaskDelete(NULL);
        return;
    }

    http.setTimeout(60000);
    http.addHeader(
        "Content-Type",
        "audio/wav"
    );
    http.addHeader(
        "X-Fumi-Device-Id",
        "fumi-main"
    );

    Serial.println(">>> SENDING WAV TO /voice...");
    Serial.println(">>> BEFORE VOICE POST");
    Serial.print(">>> WAV PTR: ");
    Serial.println((uintptr_t)wav_buffer, HEX);
    Serial.print(">>> WAV SIZE: ");
    Serial.println(wav_size);
    Serial.print(">>> FREE HEAP BEFORE POST: ");
    Serial.println(ESP.getFreeHeap());
    Serial.print(">>> LARGEST FREE HEAP BLOCK: ");
    Serial.println(ESP.getMaxAllocHeap());
    Serial.print(">>> FREE PSRAM BEFORE POST: ");
    Serial.println(ESP.getFreePsram());

int code =
    http.POST(
        wav_buffer,
        wav_size
    );

    Serial.println(">>> AFTER VOICE POST");
    Serial.print(">>> VOICE POST RESULT: ");

    Serial.print(">>> VOICE TASK STACK FREE: ");
    Serial.println(uxTaskGetStackHighWaterMark(NULL));

    Serial.println(code);
    
    char tlsError[128] = {0};

int tlsErrorCode = client.lastError(
    tlsError,
    sizeof(tlsError)
);

Serial.print(">>> TLS ERROR CODE: ");
Serial.println(tlsErrorCode);

Serial.print(">>> TLS ERROR DETAIL: ");
Serial.println(tlsError);

    Serial.print(">>> FREE HEAP AFTER POST: ");
    Serial.println(ESP.getFreeHeap());
    Serial.print(">>> FREE PSRAM AFTER POST: ");
    Serial.println(ESP.getFreePsram());

    Serial.print(">>> VOICE HTTP CODE: ");
    Serial.println(code);

    if (code > 0)
    {
        String response =
            http.getString();

        Serial.print(">>> VOICE RESPONSE: ");
        Serial.println(response);

        if (code >= 200 && code < 300)
        {
            String transcript = "";
                fumi_json_string(
                    response,
                    "transcript"
                );

            String reply =
                fumi_json_string(
                    response,
                    "reply"
                );

            String expression =
                fumi_json_string(
                    response,
                    "expression"
                );

            int expressionId = 0;

            if (expression == "happy")
                expressionId = 1;
            else if (expression == "surprised")
                expressionId = 2;
            else if (expression == "sleepy")
                expressionId = 3;
            else if (expression == "hungry")
                expressionId = 4;
            else if (expression == "very_hungry")
                expressionId = 5;
            else if (expression == "sleep")
                expressionId = 6;
            else if (expression == "sad")
                expressionId = 7;

            if (reply.length() > 0)
            {
                if (fumi_ai_mutex != NULL)
                {
                    xSemaphoreTake(
                        fumi_ai_mutex,
                        portMAX_DELAY
                    );

                    fumi_voice_pending_transcript =
                        transcript;

                    fumi_voice_pending_reply =
                        reply;

                    fumi_voice_pending_expression =
                        expressionId;

                    fumi_voice_response_ready =
                        true;

                    xSemaphoreGive(
                        fumi_ai_mutex
                    );
                }
                else
                {
                    Serial.println(">>> AI MUTEX NOT READY");
                }
            }
            else
            {
                Serial.println(">>> EMPTY VOICE REPLY");
            }
        }
        else
        {
            Serial.println(">>> VOICE SERVER ERROR");
        }
    }
    else
    {
        Serial.print(">>> VOICE HTTP ERROR: ");
        Serial.println(
            http.errorToString(code)
        );
    }

    http.end();

    free(wav_buffer);

    if (fumi_audio_buffer != NULL)
    {
        free(fumi_audio_buffer);
        fumi_audio_buffer = NULL;
    }

    fumi_audio_samples = 0;
    fumi_audio_capacity = 0;

    Serial.println(">>> FUMI VOICE UPLOAD END");
    Serial.println("================================");

    fumi_voice_upload_busy = false;

    vTaskDelete(NULL);
}

void fumi_start_voice_upload()
{
    if (fumi_voice_upload_busy)
    {
        Serial.println(">>> VOICE UPLOAD ALREADY RUNNING");
        return;
    }

    if (
        fumi_audio_buffer == NULL ||
        fumi_audio_samples < 2
    )
    {
        Serial.println(">>> NO AUDIO TO UPLOAD");
        return;
    }

    fumi_voice_upload_busy = true;

    BaseType_t result =
        xTaskCreatePinnedToCore(
            fumi_voice_upload_task,
            "Fumi Voice",
            6144,
            NULL,
            1,
            NULL,
            1
        );

    if (result != pdPASS)
    {
        Serial.println(">>> VOICE TASK CREATE FAILED");
        fumi_voice_upload_busy = false;
    }
}

void fumi_stop_recording()
{
    if (
        fumi_audio_buffer == NULL ||
        fumi_audio_samples < 2
    )
    {
        Serial.println(">>> NO AUDIO DATA");
        return;
    }

    fumi_recording = false;

    Serial.println();
    Serial.println("================================");
    Serial.println(">>> AUDIO RECORDING COMPLETE");

    Serial.print(">>> SAMPLES: ");
    Serial.println(fumi_audio_samples);

    Serial.print(">>> BYTES: ");
    Serial.println(
        fumi_audio_samples * sizeof(int16_t)
    );

    int32_t peak = 0;
    int64_t sumSquares = 0;

    for (size_t i = 0; i < fumi_audio_samples; i++)
    {
        int32_t sample =
            fumi_audio_buffer[i];

        int32_t absoluteSample =
            abs(sample);

        if (absoluteSample > peak)
            peak = absoluteSample;

        sumSquares +=
            (int64_t)sample * sample;
    }

    float rms = sqrt(
        (double)sumSquares /
        (double)fumi_audio_samples
    );

    Serial.print(">>> PEAK: ");
    Serial.println(peak);

    Serial.print(">>> RMS: ");
    Serial.println(rms);

    Serial.println("================================");

    fumi_start_voice_upload();
}

void fumi_voice_button_event(lv_event_t *e)
{
    lv_event_code_t code =
        lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED)
    {
        if (!fumi_mic_ready)
        {
            Serial.println(">>> MIC NOT READY");
            return;
        }

        if (fumi_voice_upload_busy)
        {
            Serial.println(">>> FUMI IS STILL THINKING...");
            return;
        }

        fumi_voice_pressed = true;

        fumi_voice_record_start =
            millis();

        Serial.println();
        Serial.println("================================");
        Serial.println(">>> FUMI LISTENING");
        Serial.println(">>> SPEAK NOW...");
        Serial.println("================================");

        lv_obj_t *label =
            lv_obj_get_child(
                fumi_voice_button,
                0
            );

        if (label != NULL)
        {
            lv_label_set_text(
                label,
                "LISTENING"
            );
        }

        fumi_start_recording();
    }

    else if (code == LV_EVENT_RELEASED)
    {
        if (!fumi_voice_pressed)
            return;

        fumi_voice_pressed = false;

        uint32_t duration =
            millis() -
            fumi_voice_record_start;

        Serial.println();
        Serial.println("================================");
        Serial.println(">>> FUMI STOP LISTENING");

        Serial.print(">>> DURATION: ");
        Serial.print(duration);
        Serial.println(" ms");

        Serial.println("================================");

        lv_obj_t *label =
            lv_obj_get_child(
                fumi_voice_button,
                0
            );

        if (label != NULL)
        {
            lv_label_set_text(
                label,
                "THINKING..."
            );
        }

        fumi_stop_recording();
    }
}

void fumi_blink_timer_cb(lv_timer_t * timer);
void fumi_status_gesture_event(lv_event_t *e);
void fumi_create_status_screen(lv_obj_t *screen);
void fumi_show_status_screen();
void fumi_hide_status_screen();
void fumi_load_age();
void fumi_save_age();
void fumi_update_status_values();
void fumi_age_timer_cb(lv_timer_t *timer);
void fumi_load_game_memory();
void fumi_save_game_memory();
void fumi_update_status_values();
void fumi_start_minigame();
void fumi_end_minigame();
void fumi_move_game_target();
void fumi_game_target_event(lv_event_t *e);
void fumi_game_timer_cb(lv_timer_t *timer);
void fumi_double_tap_event(lv_event_t *e);
void fumi_ai_connect_wifi();
void fumi_ai_request(const char *event_name);
void fumi_ai_timer_cb(lv_timer_t *timer);
void fumi_ai_show_message(const String &message);
void fumi_ai_apply_response(const String &response);
void fumi_ai_apply_timer_cb(lv_timer_t *timer);
void fumi_ai_request_task(void *parameter);
String fumi_json_string(const String &json, const char *key);
void fumi_voice_upload_task(void *parameter);
int fumi_json_int(const String &json, const char *key, int fallback);

void fumi_idle_timer_cb(lv_timer_t * timer);
void fumi_load_bond();
void fumi_save_bond();
void fumi_add_bond(int amount);

void Driver_Init()
{
    Flash_test();
    PWR_Init();
    BAT_Init();
    I2C_Init();
    TCA9554PWR_Init(0x00);
    Backlight_Init();
    Set_Backlight(50);
    PCF85063_Init();
    QMI8658_Init();
}
void fumi_anim_width(void *var, int32_t value)
{
    lv_obj_set_width((lv_obj_t *)var, value);
}

void fumi_anim_height(void *var, int32_t value)
{
    lv_obj_set_height((lv_obj_t *)var, value);
}

void fumi_animate_size(lv_obj_t *obj, int width, int height)
{
    // Animate WIDTH
    lv_anim_t anim_w;
    lv_anim_init(&anim_w);
    lv_anim_set_var(&anim_w, obj);
    lv_anim_set_values(&anim_w, lv_obj_get_width(obj), width);
    lv_anim_set_time(&anim_w, 180);
    lv_anim_set_exec_cb(&anim_w, fumi_anim_width);
    lv_anim_set_path_cb(&anim_w, lv_anim_path_ease_out);
    lv_anim_start(&anim_w);

    // Animate HEIGHT
    lv_anim_t anim_h;
    lv_anim_init(&anim_h);
    lv_anim_set_var(&anim_h, obj);
    lv_anim_set_values(&anim_h, lv_obj_get_height(obj), height);
    lv_anim_set_time(&anim_h, 180);
    lv_anim_set_exec_cb(&anim_h, fumi_anim_height);
    lv_anim_set_path_cb(&anim_h, lv_anim_path_ease_out);
    lv_anim_start(&anim_h);
}
void fumi_apply_expression(int expression)
{

if (fumi_expression == expression)
    return;

fumi_expression = expression;

    lv_obj_t *eye_left    = lv_obj_get_child(fumi_face, 0);
    lv_obj_t *eye_right   = lv_obj_get_child(fumi_face, 1);
    lv_obj_t *cheek_left  = lv_obj_get_child(fumi_face, 2);
    lv_obj_t *cheek_right = lv_obj_get_child(fumi_face, 3);
    lv_obj_t *mouth       = lv_obj_get_child(fumi_face, 4);

    // NORMAL
    if (expression == 0)
    {
        fumi_animate_size(eye_left, 38, 100);
        fumi_animate_size(eye_right, 38, 100);

        fumi_animate_size(cheek_left, 20, 8);
        fumi_animate_size(cheek_right, 20, 8);

        fumi_animate_size(mouth, 12, 6);
    }

    // HAPPY
    else if (expression == 1)
    {
        // shorter eyes
        fumi_animate_size(eye_left, 38, 55);
        fumi_animate_size(eye_right, 38, 55);

        // larger blush
        fumi_animate_size(cheek_left, 32, 12);
        fumi_animate_size(cheek_right, 32, 12);

        // smiling mouth
        fumi_animate_size(mouth, 30, 10);
    }

    // SURPRISED
    else if (expression == 2)
    {
        // larger eyes
        fumi_animate_size(eye_left, 48, 115);
        fumi_animate_size(eye_right, 48, 115);

        // smaller blush
        fumi_animate_size(cheek_left, 24, 8);
        fumi_animate_size(cheek_right, 24, 8);

        // O-shaped mouth
        fumi_animate_size(mouth, 22, 22);
    }

    // SLEEPY
    else if (expression == 3)
    {
        // nearly closed eyes
        fumi_animate_size(eye_left, 38, 12);
        fumi_animate_size(eye_right, 38, 12);

        // smaller blush
        fumi_animate_size(cheek_left, 20, 8);
        fumi_animate_size(cheek_right, 20, 8);

        // smaller mouth
        fumi_animate_size(mouth, 14, 6);
    }
// HUNGRY
else if (expression == 4)
{
    // slightly lowered eyes
    fumi_animate_size(eye_left, 38, 30);
    fumi_animate_size(eye_right, 38, 30);

    // slightly smaller cheeks
    fumi_animate_size(cheek_left, 18, 7);
    fumi_animate_size(cheek_right, 18, 7);

    // wider mouth
    fumi_animate_size(mouth, 26, 12);
}
// VERY HUNGRY
else if (expression == 5)
{
    // very small eyes — tired/hungry
    fumi_animate_size(eye_left, 38, 16);
    fumi_animate_size(eye_right, 38, 16);

    // small cheeks
    fumi_animate_size(cheek_left, 14, 5);
    fumi_animate_size(cheek_right, 14, 5);

    // large open mouth
    fumi_animate_size(mouth, 28, 18);
}
else if (expression == 6)
{
    // SLEEP
    fumi_animate_size(eye_left, 38, 6);
    fumi_animate_size(eye_right, 38, 6);

    fumi_animate_size(cheek_left, 12, 5);
    fumi_animate_size(cheek_right, 12, 5);

    fumi_animate_size(mouth, 10, 4);
}
else if (expression == 7)
{
    // SAD
    fumi_animate_size(eye_left, 38, 70);
    fumi_animate_size(eye_right, 38, 70);

    fumi_animate_size(cheek_left, 12, 5);
    fumi_animate_size(cheek_right, 12, 5);

    fumi_animate_size(mouth, 8, 4);
}
    // make all shapes smoother and more rounded
    lv_obj_set_style_radius(eye_left, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_radius(eye_right, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_radius(cheek_left, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_radius(cheek_right, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_radius(mouth, LV_RADIUS_CIRCLE, 0);
}
void fumi_load_bond()
{
    fumi_preferences.begin("fumi", false);
    fumi_bond = fumi_preferences.getInt("bond", 0);

    if (fumi_bond < 0)
        fumi_bond = 0;

    if (fumi_bond > 100)
        fumi_bond = 100;
}

void fumi_save_bond()
{
    fumi_preferences.putInt("bond", fumi_bond);
}

void fumi_add_bond(int amount)
{
    fumi_bond += amount;

    if (fumi_bond > 100)
        fumi_bond = 100;

    if (fumi_bond < 0)
        fumi_bond = 0;

    fumi_save_bond();
}

void fumi_load_age()
{
    // Age is persistent active time.
    // This keeps FUMI's age through restarts/reuploads.
    fumi_age_seconds = fumi_preferences.getULong("age_sec", 0);
}

void fumi_save_age()
{
    fumi_preferences.putULong("age_sec", fumi_age_seconds);
}

void fumi_update_status_values()
{
    if (fumi_status_screen == NULL)
        return;

    if (fumi_status_bond_value != NULL)
        lv_label_set_text_fmt(fumi_status_bond_value, "%d", fumi_bond);

    if (fumi_status_happiness_value != NULL)
        lv_label_set_text_fmt(fumi_status_happiness_value, "%d", fumi_happiness);

    if (fumi_status_energy_value != NULL)
        lv_label_set_text_fmt(fumi_status_energy_value, "%d", fumi_energy);

    if (fumi_status_hunger_value != NULL)
        lv_label_set_text_fmt(fumi_status_hunger_value, "%d", fumi_hunger);

    int day = (int)(fumi_age_seconds / 86400UL) + 1;

    if (fumi_status_day_value != NULL)
        lv_label_set_text_fmt(fumi_status_day_value, "DAY %d", day);

    const char *growth_stage;

    if (day <= 2)
        growth_stage = "BABY FUMI";
    else if (day <= 6)
        growth_stage = "CHILD FUMI";
    else if (day <= 13)
        growth_stage = "TEEN FUMI";
    else
        growth_stage = "ADULT FUMI";

    if (fumi_status_growth_value != NULL)
        lv_label_set_text(fumi_status_growth_value, growth_stage);
}


void fumi_age_timer_cb(lv_timer_t *timer)
{
    if (fumi_age_seconds < 0xFFFFFFFFUL - 60UL)
        fumi_age_seconds += 60UL;

    fumi_save_age();
    fumi_update_status_values();
}

void fumi_idle_timer_cb(lv_timer_t *timer)
{
    // Do not interrupt FUMI while sleeping or eating.
    if (fumi_sleeping)
        return;

    if (fumi_food != NULL)
        return;

    // State penting tetap punya prioritas
    if (fumi_hunger > 75)
        return;

    if (fumi_energy <= 20)
        return;

    if (fumi_happiness < 30)
        return;

    // Personality: makin playful, makin sering HAPPY / SURPRISED
    int roll = random(0, 100);
    int idle_expression;

    // Higher Bond makes FUMI a little more affectionate during idle.
    int happy_bias = 0;
    if (fumi_bond >= 50)
        happy_bias = 15;
    if (fumi_bond >= 80)
        happy_bias = 30;

    if (roll < (fumi_playfulness + happy_bias) && (fumi_playfulness + happy_bias) > 0)
    {
        // Playful / bonded personality
        idle_expression = random(1, 3);
    }
    else
    {
        // Calm personality
        idle_expression = random(0, 4);
    }

    fumi_apply_expression(idle_expression);

    // Randomize next idle interval: 6-12 seconds (testing)
    lv_timer_set_period(timer, random(6000, 12001));
}

void fumi_happiness_timer_cb(lv_timer_t *timer)
{
    fumi_happiness -= 1;

    if (fumi_happiness < 0)
        fumi_happiness = 0;

    if (fumi_happiness_ring != NULL)
{
    lv_arc_set_value(
        fumi_happiness_ring,
        fumi_happiness
    );
}

fumi_update_state();
}

void fumi_update_state()
{
    if (fumi_sleeping)
    {
        fumi_apply_expression(6);
        return;
    }

    if (fumi_hunger > 90)
    {
        fumi_apply_expression(5);
        return;
    }

    if (fumi_hunger > 75)
    {
        fumi_apply_expression(4);
        return;
    }

    if (fumi_energy <= 20)
    {
        fumi_apply_expression(3);
        return;
    }

    if (fumi_happiness < 30)
    {
        fumi_apply_expression(7);
        return;
    }
}

void fumi_hunger_timer_cb(lv_timer_t *timer)
{
    fumi_hunger += 1;

    if (fumi_hunger > 100)
        fumi_hunger = 100;

fumi_update_hunger_ring();
fumi_update_state();
}

void fumi_energy_timer_cb(lv_timer_t *timer)
{
    if (fumi_sleeping)
    {
        // FUMI is sleeping -> energy recovers
        fumi_energy += 5;

        if (fumi_energy >= 100)
        {
            fumi_energy = 100;
            fumi_sleeping = false;
        }
    }
    else
    {
        // FUMI is awake -> energy decreases
        fumi_energy -= 5;

        if (fumi_energy < 0)
            fumi_energy = 0;

        if (fumi_energy <= 0)
        {
            fumi_sleeping = true;
            fumi_apply_expression(6);
        }
    }

    // Update the energy ring
    lv_arc_set_value(
        fumi_energy_bar,
        fumi_energy
    );

fumi_update_state();

}

void fumi_update_hunger_ring()
{
    if (fumi_hunger_ring == NULL)
        return;

    // update hunger value
    lv_arc_set_value(
        fumi_hunger_ring,
        fumi_hunger
    );

    // =========================
    // HUNGER-BASED COLORS
    // =========================

    if (fumi_hunger <= 20)
    {
        // full
        lv_obj_set_style_arc_color(
            fumi_hunger_ring,
            lv_color_hex(0x55CC88),
            LV_PART_INDICATOR
        );
    }
    else if (fumi_hunger <= 50)
    {
        // Normal
        lv_obj_set_style_arc_color(
            fumi_hunger_ring,
            lv_color_hex(0xE8D44D),
            LV_PART_INDICATOR
        );
    }
    else if (fumi_hunger <= 75)
    {
        // starting to get hungry
        lv_obj_set_style_arc_color(
            fumi_hunger_ring,
            lv_color_hex(0xF39C4A),
            LV_PART_INDICATOR
        );
    }
    else
    {
        // very hungry
        lv_obj_set_style_arc_color(
            fumi_hunger_ring,
            lv_color_hex(0xE85B5B),
            LV_PART_INDICATOR
        );
    }
}
void fumi_feed()
{
    fumi_hunger -= 10;

    fumi_happiness += 10;

    fumi_affection += 2;
    if (fumi_affection > 100)
        fumi_affection = 100;

    fumi_add_bond(3);

    fumi_playfulness += 2;
    if (fumi_playfulness > 80)
        fumi_playfulness = 80;

    fumi_energy += 10;

if (fumi_energy > 100)
    fumi_energy = 100;

lv_arc_set_value(
    fumi_energy_bar,
    fumi_energy
);

    if (fumi_hunger < 0)
        fumi_hunger = 0;

    if (fumi_happiness > 100)
        fumi_happiness = 100;

    fumi_apply_expression(1);
    fumi_ai_request("feed");
}
void fumi_touch_event(lv_event_t *e)
{
    // =========================
    // 3X TAP → SHOW VOICE BUTTON
    // =========================

    uint32_t now = millis();

    if (now - fumi_voice_last_tap > 700)
    {
        fumi_voice_tap_count = 0;
    }

    fumi_voice_last_tap = now;
    fumi_voice_tap_count++;

    if (fumi_voice_tap_count >= 3)
{
    fumi_voice_tap_count = 0;

    if (fumi_voice_button != NULL)
    {
        if (lv_obj_has_flag(
                fumi_voice_button,
                LV_OBJ_FLAG_HIDDEN))
        {
            // SHOW
            lv_obj_clear_flag(
                fumi_voice_button,
                LV_OBJ_FLAG_HIDDEN
            );

            Serial.println(">>> FUMI VOICE BUTTON SHOWN");
        }
        else
        {
            // HIDE
            lv_obj_add_flag(
                fumi_voice_button,
                LV_OBJ_FLAG_HIDDEN
            );

            Serial.println(">>> FUMI VOICE BUTTON HIDDEN");
        }
    }
}

    if (fumi_sleeping)
    {
        // wake FUMI
        fumi_sleeping = false;
        fumi_energy = 20;

        lv_arc_set_value(
            fumi_energy_bar,
            fumi_energy
        );

        fumi_apply_expression(0);
        fumi_add_bond(1);

        return;
    }
fumi_happiness += 2;
if (fumi_happiness > 100)
    fumi_happiness = 100;

fumi_affection += 1;
if (fumi_affection > 100)
    fumi_affection = 100;

fumi_add_bond(1);

fumi_playfulness += 1;
if (fumi_playfulness > 80)
    fumi_playfulness = 80;

    // Bond reaction: closer bond produces warmer reactions
    int roll = random(0, 100);
    int expression;

    if (fumi_bond >= 80 && roll < 60)
    {
        // high Bond -> more HAPPY / SURPRISED reactions
        expression = random(1, 3);
    }
    else if (fumi_bond >= 50 && roll < 40)
    {
        // medium Bond -> occasional HAPPY reactions
        expression = 1;
    }
    else
    {
        expression = random(0, 4);
    }

    fumi_apply_expression(expression);
    fumi_ai_request("tap");
}
void fumi_long_press_event(lv_event_t *e)
{
    fumi_start_feed_animation();
}
void fumi_anim_x(void *var, int32_t value)
{
    lv_obj_set_x((lv_obj_t *)var, value);
}

void fumi_anim_y(void *var, int32_t value)
{
    lv_obj_set_y((lv_obj_t *)var, value);
}
void fumi_feed_finished(lv_anim_t *anim)
{
    if (fumi_food != NULL)
    {
        lv_obj_del(fumi_food);
        fumi_food = NULL;
    }

    // food enters -> update FUMI status
    fumi_feed();
}
void fumi_start_feed_animation()
{
    if (fumi_food != NULL)
        return;

    lv_obj_t *mouth = lv_obj_get_child(fumi_face, 4);

// FUMI opens its mouth before eating
fumi_animate_size(mouth, 28, 18);

    // =========================
    // CREATE FOOD
    // =========================

    fumi_food = lv_obj_create(fumi_face);

    lv_obj_set_size(
        fumi_food,
        32,
        32
    );

    lv_obj_set_style_bg_opa(
        fumi_food,
        LV_OPA_TRANSP,
        0
    );

    lv_obj_set_style_border_width(
        fumi_food,
        0,
        0
    );

    lv_obj_clear_flag(
        fumi_food,
        LV_OBJ_FLAG_SCROLLABLE
    );

    // =========================
    // RICE BALL BODY
    // =========================

    lv_obj_t *food_body = lv_obj_create(fumi_food);

    lv_obj_set_size(
        food_body,
        30,
        30
    );

    lv_obj_set_pos(
        food_body,
        1,
        1
    );

    lv_obj_set_style_bg_color(
        food_body,
        lv_color_hex(0xFFFFFF),
        0
    );

    lv_obj_set_style_bg_opa(
        food_body,
        LV_OPA_COVER,
        0
    );

    lv_obj_set_style_radius(
        food_body,
        LV_RADIUS_CIRCLE,
        0
    );

    lv_obj_set_style_border_width(
        food_body,
        0,
        0
    );

    // =========================
    // NORI
    // =========================

    lv_obj_t *food_nori = lv_obj_create(fumi_food);

    lv_obj_set_size(
        food_nori,
        18,
        8
    );

    lv_obj_set_pos(
        food_nori,
        7,
        12
    );

    lv_obj_set_style_bg_color(
        food_nori,
        lv_color_hex(0x263A2A),
        0
    );

    lv_obj_set_style_bg_opa(
        food_nori,
        LV_OPA_COVER,
        0
    );

    lv_obj_set_style_radius(
        food_nori,
        4,
        0
    );

    lv_obj_set_style_border_width(
        food_nori,
        0,
        0
    );

    // =========================
    // INITIAL POSITION
    // =========================

    int start_x = 190;
    int start_y = 330;

    lv_obj_set_pos(
        fumi_food,
        start_x,
        start_y
    );

    // =========================
    // MOUTH CENTER TARGET
    // =========================

    int target_x =
        lv_obj_get_x(mouth) +
        (lv_obj_get_width(mouth) / 2) -
        16;

    int target_y =
        lv_obj_get_y(mouth) +
        (lv_obj_get_height(mouth) / 2) -
        16;

    // =========================
    // X ANIMATION
    // =========================

    lv_anim_t anim_x;

    lv_anim_init(&anim_x);

    lv_anim_set_var(
        &anim_x,
        fumi_food
    );

    lv_anim_set_values(
        &anim_x,
        start_x,
        target_x
    );

    lv_anim_set_time(
        &anim_x,
        500
    );

    lv_anim_set_exec_cb(
        &anim_x,
        fumi_anim_x
    );

    lv_anim_set_path_cb(
        &anim_x,
        lv_anim_path_ease_in_out
    );

    lv_anim_start(&anim_x);

    // =========================
    // Y ANIMATION
    // =========================

    lv_anim_t anim_y;

    lv_anim_init(&anim_y);

    lv_anim_set_var(
        &anim_y,
        fumi_food
    );

    lv_anim_set_values(
        &anim_y,
        start_y,
        target_y
    );

    lv_anim_set_time(
        &anim_y,
        500
    );

    lv_anim_set_exec_cb(
        &anim_y,
        fumi_anim_y
    );

    lv_anim_set_path_cb(
        &anim_y,
        lv_anim_path_ease_in_out
    );

    // food reaches the mouth
    lv_anim_set_ready_cb(
        &anim_y,
        fumi_feed_finished
    );

    lv_anim_start(&anim_y);
}
void fumi_status_gesture_event(lv_event_t *e)
{
    if (fumi_game_active)
        return;

    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED)
    {
        lv_indev_t *indev = lv_indev_get_act();

        if (indev != NULL)
        {
            lv_point_t point;
            lv_indev_get_point(indev, &point);

            fumi_swipe_start_x = point.x;
            fumi_swipe_start_y = point.y;
        }

        return;
    }

    if (code == LV_EVENT_RELEASED)
    {
        lv_indev_t *indev = lv_indev_get_act();

        if (indev == NULL)
            return;

        lv_point_t point;
        lv_indev_get_point(indev, &point);

        int32_t delta_x = point.x - fumi_swipe_start_x;
        int32_t delta_y = point.y - fumi_swipe_start_y;

        // Swipe up from the main screen -> start mini game.
        if (delta_y < -60 &&
            abs(delta_y) > abs(delta_x) &&
            !fumi_status_open)
        {
            fumi_start_minigame();
            return;
        }

        // Horizontal navigation.
        if (delta_x < -60 && !fumi_status_open)
        {
            fumi_show_status_screen();
        }
        else if (delta_x > 60 && fumi_status_open)
        {
            fumi_hide_status_screen();
        }
    }
}

void fumi_show_status_screen()
{
    if (fumi_status_screen == NULL)
        return;

    fumi_status_open = true;

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, fumi_status_screen);
    lv_anim_set_values(&anim, 412, 0);
    lv_anim_set_time(&anim, 260);
    lv_anim_set_exec_cb(&anim, fumi_anim_x);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
}

void fumi_hide_status_screen()
{
    if (fumi_status_screen == NULL)
        return;

    fumi_status_open = false;

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, fumi_status_screen);
    lv_anim_set_values(&anim, 0, 412);
    lv_anim_set_time(&anim, 260);
    lv_anim_set_exec_cb(&anim, fumi_anim_x);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
}

void fumi_create_status_screen(lv_obj_t *screen)
{
    fumi_status_screen = lv_obj_create(screen);

    lv_obj_set_size(
        fumi_status_screen,
        412,
        412
    );

    lv_obj_set_pos(
        fumi_status_screen,
        412,
        0
    );

    lv_obj_set_style_bg_color(
        fumi_status_screen,
        lv_color_hex(0x111111),
        0
    );

    lv_obj_set_style_bg_opa(
        fumi_status_screen,
        LV_OPA_COVER,
        0
    );

    lv_obj_set_style_border_width(
        fumi_status_screen,
        0,
        0
    );

    lv_obj_set_style_radius(
        fumi_status_screen,
        0,
        0
    );

    lv_obj_clear_flag(
        fumi_status_screen,
        LV_OBJ_FLAG_SCROLLABLE
    );

    // -------------------------
    // FUMI HEADER
    // -------------------------
    lv_obj_t *title = lv_label_create(fumi_status_screen);
    lv_label_set_text(title, "FUMI");
    lv_obj_set_style_text_color(
        title,
        lv_color_hex(0xFFFFFF),
        0
    );
    lv_obj_set_style_text_font(
        title,
        &lv_font_montserrat_16,
        0
    );
    lv_obj_set_style_text_letter_space(
        title,
        4,
        0
    );
    lv_obj_align(
        title,
        LV_ALIGN_TOP_MID,
        0,
        28
    );

    // -------------------------
    // GROWTH
    // -------------------------
    lv_obj_t *growth_title = lv_label_create(fumi_status_screen);
    lv_label_set_text(growth_title, "GROWTH");
    lv_obj_set_style_text_color(
        growth_title,
        lv_color_hex(0x777777),
        0
    );
    lv_obj_set_style_text_font(
        growth_title,
        &lv_font_montserrat_16,
        0
    );
    lv_obj_align(
        growth_title,
        LV_ALIGN_TOP_MID,
        0,
        68
    );

    fumi_status_growth_value = lv_label_create(fumi_status_screen);
    lv_obj_set_style_text_color(
        fumi_status_growth_value,
        lv_color_hex(0xFFFFFF),
        0
    );
    lv_obj_set_style_text_font(
        fumi_status_growth_value,
        &lv_font_montserrat_16,
        0
    );
    lv_obj_align(
        fumi_status_growth_value,
        LV_ALIGN_TOP_MID,
        0,
        94
    );

    fumi_status_day_value = lv_label_create(fumi_status_screen);
    lv_obj_set_style_text_color(
        fumi_status_day_value,
        lv_color_hex(0x999999),
        0
    );
    lv_obj_set_style_text_font(
        fumi_status_day_value,
        &lv_font_montserrat_16,
        0
    );
    lv_obj_align(
        fumi_status_day_value,
        LV_ALIGN_TOP_MID,
        0,
        116
    );

    // -------------------------
    // STATS
    // -------------------------
    lv_obj_t *bond_label = lv_label_create(fumi_status_screen);
    lv_label_set_text(bond_label, "BOND");
    lv_obj_set_style_text_color(
        bond_label,
        lv_color_hex(0xAAAAAA),
        0
    );
    lv_obj_set_style_text_font(
        bond_label,
        &lv_font_montserrat_16,
        0
    );
    lv_obj_align(
        bond_label,
        LV_ALIGN_TOP_LEFT,
        78,
        160
    );

    fumi_status_bond_value = lv_label_create(fumi_status_screen);
    lv_obj_set_style_text_color(
        fumi_status_bond_value,
        lv_color_hex(0xFFFFFF),
        0
    );
    lv_obj_set_style_text_font(
        fumi_status_bond_value,
        &lv_font_montserrat_16,
        0
    );
    lv_obj_align(
        fumi_status_bond_value,
        LV_ALIGN_TOP_RIGHT,
        -78,
        160
    );

    lv_obj_t *happy_label = lv_label_create(fumi_status_screen);
    lv_label_set_text(happy_label, "HAPPINESS");
    lv_obj_set_style_text_color(
        happy_label,
        lv_color_hex(0xAAAAAA),
        0
    );
    lv_obj_set_style_text_font(
        happy_label,
        &lv_font_montserrat_16,
        0
    );
    lv_obj_align(
        happy_label,
        LV_ALIGN_TOP_LEFT,
        78,
        198
    );

    fumi_status_happiness_value = lv_label_create(fumi_status_screen);
    lv_obj_set_style_text_color(
        fumi_status_happiness_value,
        lv_color_hex(0xFFFFFF),
        0
    );
    lv_obj_set_style_text_font(
        fumi_status_happiness_value,
        &lv_font_montserrat_16,
        0
    );
    lv_obj_align(
        fumi_status_happiness_value,
        LV_ALIGN_TOP_RIGHT,
        -78,
        198
    );

    lv_obj_t *energy_label = lv_label_create(fumi_status_screen);
    lv_label_set_text(energy_label, "ENERGY");
    lv_obj_set_style_text_color(
        energy_label,
        lv_color_hex(0xAAAAAA),
        0
    );
    lv_obj_set_style_text_font(
        energy_label,
        &lv_font_montserrat_16,
        0
    );
    lv_obj_align(
        energy_label,
        LV_ALIGN_TOP_LEFT,
        78,
        236
    );

    fumi_status_energy_value = lv_label_create(fumi_status_screen);
    lv_obj_set_style_text_color(
        fumi_status_energy_value,
        lv_color_hex(0xFFFFFF),
        0
    );
    lv_obj_set_style_text_font(
        fumi_status_energy_value,
        &lv_font_montserrat_16,
        0
    );
    lv_obj_align(
        fumi_status_energy_value,
        LV_ALIGN_TOP_RIGHT,
        -78,
        236
    );

    lv_obj_t *hunger_label = lv_label_create(fumi_status_screen);
    lv_label_set_text(hunger_label, "HUNGER");
    lv_obj_set_style_text_color(
        hunger_label,
        lv_color_hex(0xAAAAAA),
        0
    );
    lv_obj_set_style_text_font(
        hunger_label,
        &lv_font_montserrat_16,
        0
    );
    lv_obj_align(
        hunger_label,
        LV_ALIGN_TOP_LEFT,
        78,
        274
    );

    fumi_status_hunger_value = lv_label_create(fumi_status_screen);
    lv_obj_set_style_text_color(
        fumi_status_hunger_value,
        lv_color_hex(0xFFFFFF),
        0
    );
    lv_obj_set_style_text_font(
        fumi_status_hunger_value,
        &lv_font_montserrat_16,
        0
    );
    lv_obj_align(
        fumi_status_hunger_value,
        LV_ALIGN_TOP_RIGHT,
        -78,
        274
    );

    // -------------------------
    // SWIPE HINT
    // -------------------------
    lv_obj_t *hint = lv_label_create(fumi_status_screen);
    lv_label_set_text(hint, "<  SWIPE RIGHT TO GO BACK");
    lv_obj_set_style_text_color(
        hint,
        lv_color_hex(0x666666),
        0
    );
    lv_obj_set_style_text_font(
        hint,
        &lv_font_montserrat_12,
        0
    );
    lv_obj_align(
        hint,
        LV_ALIGN_BOTTOM_MID,
        0,
        -24
    );

    fumi_update_status_values();

    lv_obj_add_event_cb(
        fumi_status_screen,
        fumi_status_gesture_event,
        LV_EVENT_PRESSED,
        NULL
    );

    lv_obj_add_event_cb(
        fumi_status_screen,
        fumi_status_gesture_event,
        LV_EVENT_RELEASED,
        NULL
    );
}

void fumi_load_game_memory()
{
    fumi_games_played = fumi_preferences.getInt("games", 0);
    fumi_games_won = fumi_preferences.getInt("wins", 0);

    if (fumi_games_played < 0)
        fumi_games_played = 0;

    if (fumi_games_won < 0)
        fumi_games_won = 0;
}

void fumi_save_game_memory()
{
    fumi_preferences.putInt("games", fumi_games_played);
    fumi_preferences.putInt("wins", fumi_games_won);
}

void fumi_move_game_target()
{
    if (!fumi_game_active || fumi_game_target == NULL)
        return;

    int x = random(32, 350);
    int y = random(62, 340);

    lv_obj_set_pos(
        fumi_game_target,
        x,
        y
    );
}

void fumi_game_target_event(lv_event_t *e)
{
    if (!fumi_game_active)
        return;

    fumi_game_score++;

    if (fumi_game_score_label != NULL)
    {
        lv_label_set_text_fmt(
            fumi_game_score_label,
            "SCORE  %d",
            fumi_game_score
        );
    }

    fumi_happiness += 2;
    if (fumi_happiness > 100)
        fumi_happiness = 100;

    fumi_playfulness += 2;
    if (fumi_playfulness > 80)
        fumi_playfulness = 80;

    fumi_add_bond(1);

    if (!fumi_sleeping)
        fumi_apply_expression(1);

    fumi_move_game_target();
}

void fumi_game_timer_cb(lv_timer_t *timer)
{
    if (!fumi_game_active)
        return;

    fumi_game_elapsed++;

    if (fumi_game_elapsed >= 10)
    {
        fumi_end_minigame();
        return;
    }

    fumi_move_game_target();
}

void fumi_start_minigame()
{
    if (fumi_game_active)
        return;

    if (fumi_sleeping)
        return;

    if (fumi_food != NULL)
        return;

    fumi_game_active = true;
    fumi_game_score = 0;
    fumi_game_elapsed = 0;
    fumi_games_played++;

    fumi_save_game_memory();

    // Pause idle behavior during the game.
    if (fumi_idle_timer != NULL)
        lv_timer_pause(fumi_idle_timer);

    fumi_game_title_label = lv_label_create(lv_scr_act());
    lv_label_set_text(
        fumi_game_title_label,
        "CATCH THE HEART"
    );
    lv_obj_set_style_text_color(
        fumi_game_title_label,
        lv_color_hex(0xFFFFFF),
        0
    );
    lv_obj_set_style_text_font(
        fumi_game_title_label,
        &lv_font_montserrat_16,
        0
    );
    lv_obj_align(
        fumi_game_title_label,
        LV_ALIGN_TOP_MID,
        0,
        18
    );

    fumi_game_score_label = lv_label_create(lv_scr_act());
    lv_label_set_text(
        fumi_game_score_label,
        "SCORE  0"
    );
    lv_obj_set_style_text_color(
        fumi_game_score_label,
        lv_color_hex(0xAAAAAA),
        0
    );
    lv_obj_set_style_text_font(
        fumi_game_score_label,
        &lv_font_montserrat_16,
        0
    );
    lv_obj_align(
        fumi_game_score_label,
        LV_ALIGN_BOTTOM_MID,
        0,
        -18
    );

    // Heart target
    // Built from simple LVGL shapes so it does not depend on
    // LV_SYMBOL_HEART being enabled in the font configuration.
    fumi_game_target = lv_obj_create(lv_scr_act());

    lv_obj_set_size(
        fumi_game_target,
        46,
        46
    );

    lv_obj_set_style_bg_opa(
        fumi_game_target,
        LV_OPA_TRANSP,
        0
    );

    lv_obj_set_style_border_width(
        fumi_game_target,
        0,
        0
    );

    lv_obj_clear_flag(
        fumi_game_target,
        LV_OBJ_FLAG_SCROLLABLE
    );

    lv_obj_add_flag(
        fumi_game_target,
        LV_OBJ_FLAG_CLICKABLE
    );

    // Heart body: rotated square + two circles.
    lv_obj_t *heart_body = lv_obj_create(fumi_game_target);
    lv_obj_set_size(
        heart_body,
        25,
        25
    );
    lv_obj_set_style_bg_color(
        heart_body,
        lv_color_hex(0xFF8BA8),
        0
    );
    lv_obj_set_style_bg_opa(
        heart_body,
        LV_OPA_COVER,
        0
    );
    lv_obj_set_style_border_width(
        heart_body,
        0,
        0
    );
    lv_obj_set_style_radius(
        heart_body,
        3,
        0
    );
    lv_obj_set_style_transform_angle(
        heart_body,
        450,
        0
    );
    lv_obj_align(
        heart_body,
        LV_ALIGN_CENTER,
        0,
        4
    );

    lv_obj_t *heart_left = lv_obj_create(fumi_game_target);
    lv_obj_set_size(
        heart_left,
        23,
        23
    );
    lv_obj_set_style_bg_color(
        heart_left,
        lv_color_hex(0xFF8BA8),
        0
    );
    lv_obj_set_style_bg_opa(
        heart_left,
        LV_OPA_COVER,
        0
    );
    lv_obj_set_style_border_width(
        heart_left,
        0,
        0
    );
    lv_obj_set_style_radius(
        heart_left,
        LV_RADIUS_CIRCLE,
        0
    );
    lv_obj_align(
        heart_left,
        LV_ALIGN_CENTER,
        -9,
        -7
    );

    lv_obj_t *heart_right = lv_obj_create(fumi_game_target);
    lv_obj_set_size(
        heart_right,
        23,
        23
    );
    lv_obj_set_style_bg_color(
        heart_right,
        lv_color_hex(0xFF8BA8),
        0
    );
    lv_obj_set_style_bg_opa(
        heart_right,
        LV_OPA_COVER,
        0
    );
    lv_obj_set_style_border_width(
        heart_right,
        0,
        0
    );
    lv_obj_set_style_radius(
        heart_right,
        LV_RADIUS_CIRCLE,
        0
    );
    lv_obj_align(
        heart_right,
        LV_ALIGN_CENTER,
        9,
        -7
    );

    lv_obj_add_event_cb(
        fumi_game_target,
        fumi_game_target_event,
        LV_EVENT_PRESSED,
        NULL
    );

    fumi_move_game_target();

    fumi_game_timer = lv_timer_create(
        fumi_game_timer_cb,
        1000,
        NULL
    );
}

void fumi_end_minigame()
{
    if (!fumi_game_active)
        return;

    fumi_game_active = false;

    if (fumi_game_timer != NULL)
    {
        lv_timer_del(fumi_game_timer);
        fumi_game_timer = NULL;
    }

    if (fumi_game_target != NULL)
    {
        lv_obj_del(fumi_game_target);
        fumi_game_target = NULL;
    }

    if (fumi_game_title_label != NULL)
    {
        lv_obj_del(fumi_game_title_label);
        fumi_game_title_label = NULL;
    }

    if (fumi_game_score_label != NULL)
    {
        lv_obj_del(fumi_game_score_label);
        fumi_game_score_label = NULL;
    }

    if (fumi_game_score >= 5)
        fumi_games_won++;

    // Game reward scales with performance.
    if (fumi_game_score > 0)
    {
        fumi_happiness += fumi_game_score;

        if (fumi_happiness > 100)
            fumi_happiness = 100;

        fumi_playfulness += fumi_game_score;

        if (fumi_playfulness > 80)
            fumi_playfulness = 80;

        fumi_add_bond(2);
    }

    fumi_energy -= 3;

    if (fumi_energy < 0)
        fumi_energy = 0;

    fumi_save_game_memory();
    fumi_ai_request("game_finished");

    if (fumi_energy_bar != NULL)
    {
        lv_arc_set_value(
            fumi_energy_bar,
            fumi_energy
        );
    }

    fumi_update_status_values();

    if (fumi_idle_timer != NULL)
        lv_timer_resume(fumi_idle_timer);

    if (!fumi_sleeping)
    {
        if (fumi_game_score >= 5)
            fumi_apply_expression(1);
        else
            fumi_apply_expression(0);
    }
}

void fumi_double_tap_event(lv_event_t *e)
{
    fumi_start_minigame();
}

void fumi_prov_event(arduino_event_t *event)
{
    switch (event->event_id)
    {
        case ARDUINO_EVENT_PROV_START:
            Serial.println(">>> FUMI BLE PROVISIONING START");
            break;

        case ARDUINO_EVENT_PROV_CRED_RECV:
            Serial.println(">>> WIFI CREDENTIAL RECEIVED");
            break;

        case ARDUINO_EVENT_PROV_CRED_SUCCESS:
            Serial.println(">>> WIFI PROVISIONING SUCCESS");
            break;

        case ARDUINO_EVENT_PROV_CRED_FAIL:
            Serial.println(">>> WIFI PROVISIONING FAILED");
            break;

        case ARDUINO_EVENT_PROV_END:
            Serial.println(">>> BLE PROVISIONING END");
            break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.print(">>> WIFI CONNECTED! IP: ");
            Serial.println(WiFi.localIP());
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println(">>> WIFI DISCONNECTED");
            break;

        default:
            break;
    }
}

void fumi_ai_connect_wifi()
{
    Serial.println();
    Serial.println("========== FUMI AI WIFI ==========");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(1000);

    Serial.println("Scanning WiFi...");

    int networks = WiFi.scanNetworks();

    Serial.print("Found ");
    Serial.print(networks);
    Serial.println(" networks.");

    for (int i = 0; i < networks; i++)
    {
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(WiFi.SSID(i));
        Serial.print(" | RSSI: ");
        Serial.println(WiFi.RSSI(i));
    }

    Serial.println();
    Serial.print("Connecting to: ");
    Serial.println(FUMI_WIFI_SSID);

    WiFi.begin(
        FUMI_WIFI_SSID,
        FUMI_WIFI_PASSWORD
    );

    int attempts = 0;

    while (
        WiFi.status() != WL_CONNECTED &&
        attempts < 30
    )
    {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    Serial.println();
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("********************************");
        Serial.println("       WIFI CONNECTED!");
        Serial.println("********************************");

        Serial.print("SSID: ");
        Serial.println(WiFi.SSID());

        Serial.print("IP: ");
        Serial.println(WiFi.localIP());

        Serial.print("RSSI: ");
        Serial.println(WiFi.RSSI());
    }
    else
    {
        Serial.println("********************************");
        Serial.println("       WIFI FAILED!");
        Serial.println("********************************");

        Serial.print("WiFi status: ");
        Serial.println(WiFi.status());
    }

    Serial.println("=================================");
}

String fumi_json_string(const String &json, const char *key)
{
    String needle = String("\"") + key + "\":\"";
    int start = json.indexOf(needle);

    if (start < 0)
        return "";

    start += needle.length();

    int end = start;

    while (end < json.length())
    {
        if (json[end] == '"' && (end == start || json[end - 1] != '\\'))
            break;

        end++;
    }

    if (end >= json.length())
        return "";

    String value = json.substring(start, end);
    value.replace("\\\"", "\"");
    value.replace("\\n", " ");
    value.replace("\\\\", "\\");

    return value;
}

int fumi_json_int(
    const String &json,
    const char *key,
    int fallback
)
{
    String needle = String("\"") + key + "\":";
    int start = json.indexOf(needle);

    if (start < 0)
        return fallback;

    start += needle.length();

    while (
        start < json.length() &&
        (json[start] == ' ' || json[start] == '\t')
    )
    {
        start++;
    }

    int end = start;

    while (
        end < json.length() &&
        (
            json[end] == '-' ||
            (json[end] >= '0' && json[end] <= '9')
        )
    )
    {
        end++;
    }

    if (end == start)
        return fallback;

    return json.substring(start, end).toInt();
}

void fumi_ai_show_message(const String &message)
{
    if (message.length() == 0)
        return;

    fumi_ai_last_message = message;

    if (fumi_ai_label == NULL)
        return;

    lv_label_set_text(
        fumi_ai_label,
        message.c_str()
    );

    lv_obj_clear_flag(
        fumi_ai_label,
        LV_OBJ_FLAG_HIDDEN
    );

    // Keep the message short and readable on the round display.
    lv_timer_t *hide_timer = lv_timer_create(
        [](lv_timer_t *timer)
        {
            if (fumi_ai_label != NULL)
            {
                lv_obj_add_flag(
                    fumi_ai_label,
                    LV_OBJ_FLAG_HIDDEN
                );
            }

            lv_timer_del(timer);
        },
        5000,
        NULL
    );
}

void fumi_ai_apply_response(const String &response)
{
    String action = fumi_json_string(
        response,
        "action"
    );

    String expression = fumi_json_string(
        response,
        "expression"
    );

    String message = fumi_json_string(
        response,
        "message"
    );

    if (expression == "happy")
        fumi_apply_expression(1);
    else if (expression == "surprised")
        fumi_apply_expression(2);
    else if (expression == "sleepy")
        fumi_apply_expression(3);
    else if (expression == "hungry")
        fumi_apply_expression(4);
    else if (expression == "very_hungry")
        fumi_apply_expression(5);
    else if (expression == "sleep")
        fumi_apply_expression(6);
    else if (expression == "sad")
        fumi_apply_expression(7);
    else
        fumi_apply_expression(0);

    if (action == "play")
    {
        fumi_happiness += 3;

        if (fumi_happiness > 100)
            fumi_happiness = 100;

        fumi_playfulness += 2;

        if (fumi_playfulness > 80)
            fumi_playfulness = 80;

        fumi_update_status_values();
    }
    else if (action == "ask_food")
    {
        if (fumi_hunger > 80)
            fumi_apply_expression(5);
        else
            fumi_apply_expression(4);
    }
    else if (action == "sleep")
    {
        if (fumi_energy <= 20)
        {
            fumi_sleeping = true;
            fumi_apply_expression(6);
        }
    }

    fumi_ai_show_message(message);
}

void fumi_ai_request_task(void *parameter)
{
    Serial.println(">>> AI TASK START");

    char event_name[32];
    strncpy(
        event_name,
        (const char *)parameter,
        sizeof(event_name) - 1
    );
    event_name[sizeof(event_name) - 1] = '\0';

    free(parameter);

fumi_ai_busy = true;

{
WiFiClientSecure client;
client.setInsecure();

Serial.print(">>> FREE HEAP: ");
Serial.println(ESP.getFreeHeap());

Serial.print(">>> FREE PSRAM: ");
Serial.println(ESP.getFreePsram());

HTTPClient http;

Serial.println(">>> BEFORE HTTP BEGIN");
Serial.print("URL: ");
Serial.println(FUMI_BRAIN_URL);
Serial.print("WiFi IP: ");
Serial.println(WiFi.localIP());

if (!http.begin(
        client,
        "fumi-brain-production.up.railway.app",
        443,
        "/brain",
        true
    ))
{
    Serial.println(">>> HTTP BEGIN FAILED");

    fumi_ai_busy = false;
    return;
}

Serial.println(">>> HTTP BEGIN OK");

http.setTimeout(30000);

http.addHeader(
    "Content-Type",
    "application/json"
);

Serial.println(">>> BEFORE BUILD BODY");

    // Snapshot the state before the request.
    int hunger = fumi_hunger;
    int happiness = fumi_happiness;
    int energy = fumi_energy;
    int bond = fumi_bond;
    int affection = fumi_affection;
    int playfulness = fumi_playfulness;
    bool sleeping = fumi_sleeping;
    int expression = fumi_expression;
    uint32_t age_seconds = fumi_age_seconds;

    String body = "{";
    body += "\"event\":\"";
    body += event_name;
    body += "\",";
    body += "\"hunger\":";
    body += String(hunger);
    body += ",";
    body += "\"happiness\":";
    body += String(happiness);
    body += ",";
    body += "\"energy\":";
    body += String(energy);
    body += ",";
    body += "\"bond\":";
    body += String(bond);
    body += ",";
    body += "\"affection\":";
    body += String(affection);
    body += ",";
    body += "\"playfulness\":";
    body += String(playfulness);
    body += ",";
    body += "\"sleeping\":";
    body += (sleeping ? "true" : "false");
    body += ",";
    body += "\"expression\":";
    body += String(expression);
    body += ",";
    body += "\"age_seconds\":";
    body += String(age_seconds);
    body += "}";

Serial.println(">>> BEFORE HTTP POST");

int code = http.POST(body);

Serial.print(">>> HTTP RESPONSE CODE: ");
Serial.println(code);

Serial.print(">>> HTTP ERROR: ");
Serial.println(http.errorToString(code));

    if (code > 0)
    {
        String response = http.getString();

        if (code >= 200 && code < 300)
        {
            if (fumi_ai_mutex != NULL)
            {
                xSemaphoreTake(
                    fumi_ai_mutex,
                    portMAX_DELAY
                );

                fumi_ai_pending_response = response;
                fumi_ai_response_ready = true;

                xSemaphoreGive(
                    fumi_ai_mutex
                );
            }
        }
    }

    http.end();

Serial.print(">>> HEAP AFTER HTTP END: ");
Serial.println(ESP.getFreeHeap());

} // cleanup HTTP/TLS objects

fumi_ai_busy = false;

vTaskDelete(NULL);
}

void fumi_ai_request(const char *event_name)
{
    if (fumi_ai_busy)
        return;

    if (
        WiFi.status() != WL_CONNECTED ||
        strlen(FUMI_BRAIN_URL) < 10 ||
        strstr(FUMI_BRAIN_URL, "YOUR-RAILWAY") != NULL
    )
    {
        return;
    }

    if (fumi_game_active)
        return;

    char *event_copy = (char *)malloc(32);

    if (event_copy == NULL)
        return;

    strncpy(
        event_copy,
        event_name,
        31
    );

    event_copy[31] = '\0';

    Serial.print(">>> HEAP BEFORE CREATE TASK: ");
    Serial.println(ESP.getFreeHeap());

    xTaskCreatePinnedToCore(
        fumi_ai_request_task,
        "Fumi AI",
        8192,
        event_copy,
        1,
        NULL,
        1
    );
}

void fumi_ai_apply_timer_cb(lv_timer_t *timer)
{
    // Voice response arrives from the HTTP task.
    // Apply LVGL changes and start TTS here, on the UI task.
    if (fumi_voice_response_ready)
    {
        String reply;
        String transcript;
        int expressionId = 0;

        if (fumi_ai_mutex != NULL)
        {
            xSemaphoreTake(
                fumi_ai_mutex,
                0
            );

            reply =
                fumi_voice_pending_reply;

            transcript =
                fumi_voice_pending_transcript;

            expressionId =
                fumi_voice_pending_expression;

            fumi_voice_pending_reply = "";
            fumi_voice_pending_transcript = "";
            fumi_voice_response_ready = false;

            xSemaphoreGive(
                fumi_ai_mutex
            );
        }

        Serial.println(">>> VOICE RESPONSE READY");

        Serial.print(">>> TRANSCRIPT: ");
        Serial.println(transcript);

        Serial.print(">>> FUMI REPLY: ");
        Serial.println(reply);

        fumi_apply_expression(
            expressionId
        );

        fumi_ai_show_message(reply);

        Volume_adjustment(21);

        bool ttsOk =
            audio.connecttospeech(
            reply.c_str(),
            "en"
        );

        if (ttsOk)
            Serial.println(">>> FUMI TTS STARTED");
        else
            Serial.println(">>> FUMI TTS REQUEST FAILED");

        if (fumi_voice_button != NULL)
        {
            lv_obj_t *label =
                lv_obj_get_child(
                    fumi_voice_button,
                    0
                );

            if (label != NULL)
                lv_label_set_text(
                    label,
                    "TALK"
                );
        }
    }

    if (!fumi_ai_response_ready)
        return;

    String response;

    if (fumi_ai_mutex != NULL)
    {
        xSemaphoreTake(
            fumi_ai_mutex,
            0
        );

        response = fumi_ai_pending_response;
        fumi_ai_pending_response = "";
        fumi_ai_response_ready = false;

        xSemaphoreGive(
            fumi_ai_mutex
        );
    }

    if (response.length() > 0)
        fumi_ai_apply_response(response);
}

void fumi_ai_timer_cb(lv_timer_t *timer)
{
    fumi_ai_request("heartbeat");
}

void Fumi_UI()
{
  // Background
  lv_obj_t * screen = lv_scr_act();

  lv_obj_set_style_bg_color(
    screen,
    lv_color_hex(0x111111),
    0
  );

  lv_obj_set_style_bg_opa(
    screen,
    LV_OPA_COVER,
    0
  );


  // =========================
  // FUMI FACE
  // =========================

  lv_obj_t * face = lv_obj_create(screen);

fumi_face = face;

// AI speech bubble
fumi_ai_label = lv_label_create(screen);
lv_label_set_text(
    fumi_ai_label,
    ""
);
lv_obj_set_style_text_color(
    fumi_ai_label,
    lv_color_hex(0x111111),
    0
);
lv_obj_set_style_text_font(
    fumi_ai_label,
    &lv_font_montserrat_12,
    0
);
lv_obj_set_style_bg_color(
    fumi_ai_label,
    lv_color_hex(0xFFF1D6),
    0
);
lv_obj_set_style_bg_opa(
    fumi_ai_label,
    LV_OPA_COVER,
    0
);
lv_obj_set_style_radius(
    fumi_ai_label,
    14,
    0
);
lv_obj_set_style_pad_left(
    fumi_ai_label,
    10,
    0
);
lv_obj_set_style_pad_right(
    fumi_ai_label,
    10,
    0
);
lv_obj_set_style_pad_top(
    fumi_ai_label,
    6,
    0
);
lv_obj_set_style_pad_bottom(
    fumi_ai_label,
    6,
    0
);
lv_obj_set_width(
    fumi_ai_label,
    190
);
lv_label_set_long_mode(
    fumi_ai_label,
    LV_LABEL_LONG_WRAP
);
lv_obj_align(
    fumi_ai_label,
    LV_ALIGN_TOP_MID,
    0,
    18
);
lv_obj_add_flag(
    fumi_ai_label,
    LV_OBJ_FLAG_HIDDEN
);

  lv_obj_set_size(face, 300, 300);
  lv_obj_center(face);
// =========================
// HUNGER RING
// =========================

fumi_hunger_ring = lv_arc_create(screen);

lv_obj_set_size(
    fumi_hunger_ring,
    360,
    360
);

lv_obj_center(
    fumi_hunger_ring
);

// Background ring
lv_arc_set_bg_angles(
    fumi_hunger_ring,
    0,
    360
);

// hunger value
lv_arc_set_range(
    fumi_hunger_ring,
    0,
    100
);

lv_arc_set_value(
    fumi_hunger_ring,
    fumi_hunger
);

// remove knob
lv_obj_remove_style(
    fumi_hunger_ring,
    NULL,
    LV_PART_KNOB
);

// ring background color
lv_obj_set_style_arc_color(
    fumi_hunger_ring,
    lv_color_hex(0x333333),
    LV_PART_MAIN
);

// hunger progress color
lv_obj_set_style_arc_color(
    fumi_hunger_ring,
    lv_color_hex(0xFFFFFF),
    LV_PART_INDICATOR
);

// ring thickness
lv_obj_set_style_arc_width(
    fumi_hunger_ring,
    5,
    LV_PART_MAIN
);

lv_obj_set_style_arc_width(
    fumi_hunger_ring,
    5,
    LV_PART_INDICATOR
);

// keep the ring from covering FUMI
lv_obj_move_background(
    fumi_hunger_ring
);

fumi_update_hunger_ring();
fumi_energy_bar = lv_arc_create(screen);

lv_obj_set_size(
    fumi_energy_bar,
    320,
    320
);

lv_obj_center(fumi_energy_bar);

lv_arc_set_bg_angles(
    fumi_energy_bar,
    0,
    360
);

lv_arc_set_range(
    fumi_energy_bar,
    0,
    100
);

lv_arc_set_value(
    fumi_energy_bar,
    fumi_energy
);

lv_obj_remove_style(
    fumi_energy_bar,
    NULL,
    LV_PART_KNOB
);

lv_obj_set_style_arc_width(
    fumi_energy_bar,
    4,
    LV_PART_MAIN
);

lv_obj_set_style_arc_width(
    fumi_energy_bar,
    4,
    LV_PART_INDICATOR
);

lv_obj_set_style_arc_color(
    fumi_energy_bar,
    lv_color_hex(0x222222),
    LV_PART_MAIN
);

lv_obj_set_style_arc_color(
    fumi_energy_bar,
    lv_color_hex(0x6CCB8E),
    LV_PART_INDICATOR
);

fumi_happiness_ring = lv_arc_create(screen);

lv_obj_set_size(
    fumi_happiness_ring,
    340,
    340
);

lv_obj_center(fumi_happiness_ring);

lv_arc_set_bg_angles(
    fumi_happiness_ring,
    0,
    360
);

lv_arc_set_range(
    fumi_happiness_ring,
    0,
    100
);

lv_arc_set_value(
    fumi_happiness_ring,
    fumi_happiness
);

lv_obj_remove_style(
    fumi_happiness_ring,
    NULL,
    LV_PART_KNOB
);

lv_obj_set_style_arc_width(
    fumi_happiness_ring,
    3,
    LV_PART_MAIN
);

lv_obj_set_style_arc_width(
    fumi_happiness_ring,
    3,
    LV_PART_INDICATOR
);

lv_obj_set_style_arc_color(
    fumi_happiness_ring,
    lv_color_hex(0x222222),
    LV_PART_MAIN
);

lv_obj_set_style_arc_color(
    fumi_happiness_ring,
    lv_color_hex(0xFF8BA8),
    LV_PART_INDICATOR
);

lv_obj_move_background(
    fumi_happiness_ring
);

lv_obj_move_background(
    fumi_energy_bar
);

fumi_hunger_timer = lv_timer_create(
    fumi_hunger_timer_cb,
    5000,
    NULL
);

fumi_energy_timer = lv_timer_create(
    fumi_energy_timer_cb,
    10000,
    NULL
);

fumi_happiness_timer = lv_timer_create(
    fumi_happiness_timer_cb,
    10000,
    NULL
);

fumi_idle_timer = lv_timer_create(
    fumi_idle_timer_cb,
    8000,
    NULL
);

  lv_obj_set_style_bg_color(
    face,
    lv_color_hex(0xFFF1D6),
    0
  );

  lv_obj_set_style_bg_opa(
    face,
    LV_OPA_COVER,
    0
  );

  lv_obj_set_style_radius(
    face,
    LV_RADIUS_CIRCLE,
    0
  );

  lv_obj_set_style_border_width(
    face,
    0,
    0
  );

  lv_obj_clear_flag(
    face,
    LV_OBJ_FLAG_SCROLLABLE
  );


  // =========================
  // LEFT EYE
  // =========================

  lv_obj_t * eye_left = lv_obj_create(face);

  lv_obj_set_size(
    eye_left,
    38,
    100
  );

  lv_obj_set_pos(
    eye_left,
    78,
    75
  );

  lv_obj_set_style_bg_color(
    eye_left,
    lv_color_hex(0x111111),
    0
  );

  lv_obj_set_style_bg_opa(
    eye_left,
    LV_OPA_COVER,
    0
  );

  lv_obj_set_style_radius(
    eye_left,
    LV_RADIUS_CIRCLE,
    0
  );

  lv_obj_set_style_border_width(
    eye_left,
    0,
    0
  );


  // =========================
  // RIGHT EYE
  // =========================

  lv_obj_t * eye_right = lv_obj_create(face);

  lv_obj_set_size(
    eye_right,
    38,
    100
  );

  lv_obj_set_pos(
    eye_right,
    184,
    75
  );

  lv_obj_set_style_bg_color(
    eye_right,
    lv_color_hex(0x111111),
    0
  );

  lv_obj_set_style_bg_opa(
    eye_right,
    LV_OPA_COVER,
    0
  );

  lv_obj_set_style_radius(
    eye_right,
    LV_RADIUS_CIRCLE,
    0
  );

  lv_obj_set_style_border_width(
    eye_right,
    0,
    0
  );


  // =========================
  // LEFT CHEEK
  // =========================

  lv_obj_t * cheek_left = lv_obj_create(face);

  lv_obj_set_size(
    cheek_left,
    45,
    22
  );

  lv_obj_set_pos(
    cheek_left,
    35,
    190
  );

  lv_obj_set_style_bg_color(
    cheek_left,
    lv_color_hex(0xF4A6A6),
    0
  );

  lv_obj_set_style_bg_opa(
    cheek_left,
    LV_OPA_COVER,
    0
  );

  lv_obj_set_style_radius(
    cheek_left,
    LV_RADIUS_CIRCLE,
    0
  );

  lv_obj_set_style_border_width(
    cheek_left,
    0,
    0
  );


  // =========================
  // RIGHT CHEEK
  // =========================

  lv_obj_t * cheek_right = lv_obj_create(face);

  lv_obj_set_size(
    cheek_right,
    45,
    22
  );

  lv_obj_set_pos(
    cheek_right,
    220,
    190
  );

  lv_obj_set_style_bg_color(
    cheek_right,
    lv_color_hex(0xF4A6A6),
    0
  );

  lv_obj_set_style_bg_opa(
    cheek_right,
    LV_OPA_COVER,
    0
  );

  lv_obj_set_style_radius(
    cheek_right,
    LV_RADIUS_CIRCLE,
    0
  );

  lv_obj_set_style_border_width(
    cheek_right,
    0,
    0
  );


  // =========================
  // MOUTH
  // =========================

  lv_obj_t * mouth = lv_obj_create(face);

  lv_obj_set_size(
    mouth,
    45,
    14
  );

  lv_obj_set_pos(
    mouth,
    128,
    225
  );

  lv_obj_set_style_bg_color(
    mouth,
    lv_color_hex(0x111111),
    0
  );

  lv_obj_set_style_bg_opa(
    mouth,
    LV_OPA_COVER,
    0
  );

  lv_obj_set_style_radius(
    mouth,
    LV_RADIUS_CIRCLE,
    0
  );

  lv_obj_set_style_border_width(
    mouth,
    0,
    0
  );


  // =========================
  // TOUCH
  // =========================

lv_obj_add_event_cb(
    fumi_face,
    fumi_touch_event,
    LV_EVENT_PRESSED,
    fumi_face
);
lv_obj_add_event_cb(
    fumi_face,
    fumi_long_press_event,
    LV_EVENT_LONG_PRESSED,
    fumi_face
);

fumi_apply_expression(0);

// Start automatic blinking
fumi_blink_timer = lv_timer_create(
    fumi_blink_timer_cb,
    random(2500, 6000),
    NULL
);

// =========================
// STATUS SCREEN / SWIPE
// =========================
fumi_create_status_screen(screen);

lv_obj_add_event_cb(
    screen,
    fumi_status_gesture_event,
    LV_EVENT_PRESSED,
    NULL
);

lv_obj_add_event_cb(
    screen,
    fumi_status_gesture_event,
    LV_EVENT_RELEASED,
    NULL
);
// =========================
// VOICE / PUSH-TO-TALK
// =========================

fumi_voice_button = lv_btn_create(screen);

lv_obj_add_flag(
    fumi_voice_button,
    LV_OBJ_FLAG_HIDDEN
);

lv_obj_set_size(
    fumi_voice_button,
    90,
    42
);

lv_obj_align(
    fumi_voice_button,
    LV_ALIGN_CENTER,
    0,
    0
);

lv_obj_set_style_radius(
    fumi_voice_button,
    20,
    0
);

lv_obj_set_style_bg_color(
    fumi_voice_button,
    lv_color_hex(0x111111),
    LV_PART_MAIN
);

lv_obj_set_style_bg_opa(
    fumi_voice_button,
    LV_OPA_COVER,
    LV_PART_MAIN
);

lv_obj_set_style_border_width(
    fumi_voice_button,
    1,
    LV_PART_MAIN
);

lv_obj_set_style_border_color(
    fumi_voice_button,
    lv_color_hex(0x444444),
    LV_PART_MAIN
);

lv_obj_t *voice_label = lv_label_create(
    fumi_voice_button
);

lv_label_set_text(
    voice_label,
    "TALK"
);

lv_obj_set_style_text_color(
    voice_label,
    lv_color_hex(0xFFFFFF),
    0
);

lv_obj_center(
    voice_label
);

lv_obj_add_event_cb(
    fumi_voice_button,
    fumi_voice_button_event,
    LV_EVENT_PRESSED,
    NULL
);

lv_obj_add_event_cb(
    fumi_voice_button,
    fumi_voice_button_event,
    LV_EVENT_RELEASED,
    NULL
);
}
void fumi_blink_timer_cb(lv_timer_t * timer)
{
    static bool blinking = false;

    lv_obj_t *eye_left  = lv_obj_get_child(fumi_face, 0);
    lv_obj_t *eye_right = lv_obj_get_child(fumi_face, 1);

    if (!blinking)
    {
        // CLOSE EYES
        lv_obj_set_size(eye_left, 38, 8);
        lv_obj_set_size(eye_right, 38, 8);

        blinking = true;

        // Open again after 120 ms
        lv_timer_set_period(timer, 120);
    }
    else
    {
        // OPEN EYES ACCORDING TO CURRENT EXPRESSION

        // NORMAL
        if (fumi_expression == 0)
        {
            lv_obj_set_size(eye_left, 38, 100);
            lv_obj_set_size(eye_right, 38, 100);
        }

        // HAPPY
        else if (fumi_expression == 1)
        {
            lv_obj_set_size(eye_left, 38, 55);
            lv_obj_set_size(eye_right, 38, 55);
        }

        // SURPRISED
        else if (fumi_expression == 2)
        {
            lv_obj_set_size(eye_left, 48, 115);
            lv_obj_set_size(eye_right, 48, 115);
        }

        // SLEEPY
        else if (fumi_expression == 3)
        {
            lv_obj_set_size(eye_left, 38, 12);
            lv_obj_set_size(eye_right, 38, 12);
        }

        else if (fumi_expression == 4)
{
    lv_obj_set_size(eye_left, 38, 28);
    lv_obj_set_size(eye_right, 38, 28);
}
else if (fumi_expression == 5)
{
    lv_obj_set_size(eye_left, 38, 18);
    lv_obj_set_size(eye_right, 38, 18);
}

        blinking = false;

        // Random next blink: 2.5 - 6 seconds
        uint32_t next_blink = random(2500, 6000);
        lv_timer_set_period(timer, next_blink);
    }
}

void setup()
{
    Serial.begin(115200);

    heap_caps_malloc_extmem_enable(4096);

    Serial.print("PSRAM FOUND: ");
    Serial.println(psramFound());

    Serial.print("PSRAM SIZE: ");
    Serial.println(ESP.getPsramSize());

    Serial.println("========== FUMI SETUP START ==========");
    delay(3000);

Driver_Init();
Serial.println(">>> DRIVER INIT DONE");

SD_Init();
Serial.println(">>> SD INIT DONE");

Audio_Init();
Serial.println(">>> AUDIO INIT DONE");

fumi_mic_init();

// MIC_Init();

Serial.println(">>> BEFORE LCD INIT");
LCD_Init();
Serial.println(">>> LCD INIT DONE");

Serial.println(">>> BEFORE LVGL INIT");

Lvgl_Init();

Serial.println(">>> AFTER LVGL INIT");

  randomSeed(micros());

  fumi_load_bond();
  fumi_load_age();
  fumi_load_game_memory();

  Fumi_UI();
Serial.println(">>> FUMI UI DONE");

fumi_ai_mutex = xSemaphoreCreateMutex();
Serial.println(">>> AI MUTEX DONE");

  fumi_ai_apply_timer = lv_timer_create(
    fumi_ai_apply_timer_cb,
    100,
    NULL
  );

  Serial.println(">>> BEFORE FUMI WIFI");
  Serial.println(">>> CALLING FUMI WIFI");
  fumi_ai_connect_wifi();
  Serial.println(">>> AFTER FUMI WIFI");

// fumi_ai_timer = lv_timer_create(
//     fumi_ai_timer_cb,
//     60000,
//     NULL
// );

  fumi_ai_request("boot");

  fumi_age_timer = lv_timer_create(
    fumi_age_timer_cb,
    60000,
    NULL
  );

  fumi_update_status_values();

  xTaskCreatePinnedToCore(
    Driver_Loop,
    "Other Driver task",
    2048,
    NULL,
    3,
    NULL,
    0
  );
}

void loop()
{
  Lvgl_Loop();
  fumi_recording_loop();

  if (Serial.available())
  {
    char command = Serial.read();

    if (command == 'r' || command == 'R')
    {
      fumi_mic_test();
    }
    else if (command == 's' || command == 'S')
    {
      Serial.println(">>> FUMI SPEAKER TEST START");

      Volume_adjustment(21);

      bool ok = audio.connecttospeech(
        "Hello Rey. I am Fumi. Can you hear me?",
        "en"
      );

      if (ok)
      {
        Serial.println(">>> TTS REQUEST ACCEPTED");
      }
      else
      {
        Serial.println(">>> TTS REQUEST FAILED");
      }
    }
  }

  vTaskDelay(pdMS_TO_TICKS(5));
}