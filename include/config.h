#pragma once

#include <Arduino.h>

namespace Config {
constexpr char DEVICE_NAME[] = "ChainOSCPad MIDI";
constexpr uint8_t ROW_PINS[] = {D0, D1, D2, D3};
constexpr uint8_t COL_PINS[] = {D4, D5, D6};
constexpr uint8_t ENCODER_A = D7;
constexpr uint8_t ENCODER_B = D8;
constexpr uint8_t ENCODER_BUTTON = D9;
constexpr uint8_t LED = D10;
constexpr bool LED_ACTIVE_HIGH = true;

// Row-major: Key 1 = ROW0/COL0, Key 12 = ROW3/COL2.
constexpr uint8_t NOTES[] = {60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71};
constexpr uint8_t CHANNEL = 1;  // 1..16
constexpr uint8_t VELOCITY = 100;  // Switch matrix has no velocity sensor.
constexpr uint8_t ENCODER_CC = 1;  // Modulation
constexpr uint8_t ENCODER_INITIAL = 0;
constexpr uint8_t ENCODER_STEP = 1;
constexpr int ENCODER_DIRECTION = 1;  // -1 reverses direction
constexpr int ENCODER_TRANSITIONS = 4;  // 1, 2 or 4 per detent
constexpr uint8_t BUTTON_CC = 64;  // Sustain: held=127, released=0
constexpr uint32_t SCAN_INTERVAL_US = 1000;
constexpr uint32_t DEBOUNCE_MS = 5;

static_assert(CHANNEL >= 1 && CHANNEL <= 16, "Invalid MIDI channel");
static_assert(VELOCITY >= 1 && VELOCITY <= 127, "Invalid velocity");
static_assert(ENCODER_CC < 120 && BUTTON_CC < 120 && ENCODER_CC != BUTTON_CC,
              "Use distinct non-channel-mode controllers");
static_assert(ENCODER_INITIAL <= 127 && ENCODER_STEP >= 1 && ENCODER_STEP <= 127,
              "Invalid encoder range");
static_assert(ENCODER_DIRECTION == 1 || ENCODER_DIRECTION == -1, "Invalid direction");
static_assert(ENCODER_TRANSITIONS == 1 || ENCODER_TRANSITIONS == 2 ||
              ENCODER_TRANSITIONS == 4, "Invalid encoder resolution");
static_assert(sizeof(NOTES) == 12, "Exactly twelve notes required");
constexpr bool validNotes() {
  for (unsigned i = 0; i < 12; ++i) {
    if (NOTES[i] > 127) return false;
    for (unsigned j = 0; j < i; ++j) if (NOTES[i] == NOTES[j]) return false;
  }
  return true;
}
static_assert(validNotes(), "Notes must be unique and in 0..127");
}  // namespace Config
