#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <Arduino.h>
#include <BLEMidi.h>
#include <FastLED.h>
#include "Button.h"

/**
 * 必要なArduinoライブラリ
 * ESP32-BLE_MIDI
 *   https://github.com/max22-/ESP32-BLE-MIDI
 * FastLED
 *   https://github.com/FastLED/FastLED
 * */

#define BLE_DEVICE_NAME "PARIPI-YEI-01"
#define PIN_LEDS 19
#define NUM_LEDS 76 // 16 * 2 + 22 * 2
#define MAX_BRIGHTNESS 255
#define DEF_SATURATION 64
#define MAX_HUE 360
#define MAX_SATURATION 255
#define MAX_PERFORMANCE_STEP 10

#define MIDI_CC_DURATION 2
#define MIDI_CC_COLOR_HUE 3 // 色相
#define MIDI_CC_COLOR_SATURATION 4 // 彩度
#define MIDI_VALUE_MAX 127

#define MODE_PARIPI 0
#define MODE_GAMING 1
#define MODE_BLEMIDI 2


//-------------------------------
uint8_t brightness = MAX_BRIGHTNESS;
CRGB leds[NUM_LEDS];
int hue;
uint8_t saturation = DEF_SATURATION;
uint8_t duration = 100;
uint16_t latestNoteOnTimestamp = 0;
uint16_t currentNoteOnTimestamp = 0;
bool isPlaying = false;
uint8_t playIndex = 0;

uint8_t gamingHue = 0;

byte mode = MODE_PARIPI; 

Button Btn = Button(39, true, 10);
//-------------------------------

void connected();
void onNoteOn(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t timestamp);
void onNoteOff(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t timestamp);
void onControlChange(uint8_t channel, uint8_t controller, uint8_t value, uint16_t timestamp);

void performanceTask(void *pvParameters);

//-------------------------------

void setup() {
    Serial.begin(115200);
    // BLE MIDIの初期化・起動
    BLEMidiServer.begin(BLE_DEVICE_NAME);
    BLEMidiServer.setOnConnectCallback(connected);
    BLEMidiServer.setOnDisconnectCallback([](){     // To show how to make a callback with a lambda function
        Serial.println("Disconnected"); 
    });
    BLEMidiServer.setNoteOnCallback(onNoteOn);
    BLEMidiServer.setNoteOffCallback(onNoteOff);
    BLEMidiServer.setControlChangeCallback(onControlChange);
    //BLEMidiServer.enableDebugging();
    
    // LED初期化
    FastLED.addLeds<NEOPIXEL, PIN_LEDS>(leds, NUM_LEDS);
    FastLED.clear();  // clear all pixel data
    FastLED.show();
    
    // 演出(LED制御)用のタスクを起動
    xTaskCreatePinnedToCore(
        performanceTask,
        "performanceTask",
        8192,
        NULL,
        1,
        NULL,
        0
    );

    randomSeed(analogRead(0)); 

    // 起動時はスタンドアローン
    mode = MODE_PARIPI; 
}

void loop() {
    if (BLEMidiServer.isConnected()) {
        Serial.println("connected");
    }
    Btn.read();
    if (Btn.wasPressed()) {
      // モード切り替え(Standalone Paripi -> Standalone GAMING -> BLE MIDI)
      mode++;
      if (MODE_BLEMIDI < mode) {
        mode = MODE_PARIPI;
      }
      // モード切り替え時に、BLEの演出用のデータをクリア
      isPlaying = false;
      playIndex = 0;
    }
    delay(100);
}

//-------------------------------

void connected()
{
  Serial.println("Connected");
}

void onNoteOn(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t timestamp) {
    Serial.printf("Received note on : channel %d, note %d, velocity %d (timestamp %dms)\n", channel, note, velocity, timestamp);
    brightness = (uint8_t)((float)velocity * (float)MAX_BRIGHTNESS / (float)MIDI_VALUE_MAX);
    latestNoteOnTimestamp = timestamp;
    isPlaying = true; // 演出の再生開始をセット
}

void onNoteOff(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t timestamp)
{
    Serial.printf("Received note off : channel %d, note %d, velocity %d (timestamp %dms)\n", channel, note, velocity, timestamp);
    // note offで演出を停止する処理。制御出来に微妙なのでコメントアウト
//    if (isPlaying) {
//        isPlaying = false;
//        playIndex = 0;
//    }
}

void onControlChange(uint8_t channel, uint8_t controller, uint8_t value, uint16_t timestamp)
{
    Serial.printf("Received control change : channel %d, controller %d, value %d (timestamp %dms)\n", channel, controller, value, timestamp);
    switch (controller) {
        case MIDI_CC_DURATION: // 1演出あたりの長さを設定
            duration = ((uint8_t)value * 100.f / (float)MIDI_VALUE_MAX) + 10;
            break;
        case MIDI_CC_COLOR_HUE: // LEDの色を設定
            hue = (uint8_t)value * (float)MAX_HUE / (float)MIDI_VALUE_MAX;
            break;
        case MIDI_CC_COLOR_SATURATION: // LEDの彩度を設定
            saturation = (uint8_t)value * (float)MAX_SATURATION / (float)MIDI_VALUE_MAX;
            break;
    }
}

uint8_t gamingHueIndex = 0;

/** 演出処理のタスク */
void performanceTask(void *pvParameters) {
    while(1){
        if (mode == MODE_PARIPI) {
            // スタンドアロンモード/パリピ
            for (int i = 0; i < NUM_LEDS; i = i + 2) {
                // 1st
                hue = (int)random(0, 128) * 2;
                leds[i] = CHSV(hue, SATURATION, 128);
        
                // 2nd
                hue = (int)random(0, 16) * 16;
                leds[i + 1] = CHSV(hue, SATURATION, 128);
            }
            FastLED.show();
            vTaskDelay(50);
        } else if (mode == MODE_GAMING) {
            // スタンドアロンモード/ゲーミング
            uint8_t hue = gamingHueIndex;
            for (int i = 0; i < NUM_LEDS; i++) {
                hue += 10;
                if (255 < hue) {
                  hue - 0;
                }
                leds[i] = CHSV(hue, SATURATION, 128);
            }
            FastLED.show();
            gamingHueIndex+=20;
            if (255 < gamingHueIndex) {
                gamingHueIndex = 0;
            }
            vTaskDelay(50);
        } else if (mode == MODE_BLEMIDI && isPlaying) {
            // BLEモード
            if (currentNoteOnTimestamp != latestNoteOnTimestamp) { // 新着noteOn有り = 今の演出をキャンセル
                playIndex = 0;
                currentNoteOnTimestamp = latestNoteOnTimestamp;
            }
            // playIndexに応じて光をフェードアウトする
            uint8_t v = (int)((float)brightness * (1.f - (float)( (float)playIndex / (float)MAX_PERFORMANCE_STEP)));
            Serial.print(v);
            Serial.print(", ");
            Serial.println(playIndex);
            for (int i = 0; i < NUM_LEDS; i++)
            {
                leds[i] = CHSV(hue, saturation, v);
            }

            playIndex++;
            if (MAX_PERFORMANCE_STEP <= playIndex) { // 最後のstep = 演出終了
                isPlaying = false; 
                FastLED.clear();
            }
            FastLED.show();
            vTaskDelay(duration);
        } else {
            FastLED.clear();
            FastLED.show();
            vTaskDelay(10);
        }
    }
}
