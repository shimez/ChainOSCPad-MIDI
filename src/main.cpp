#include <Arduino.h>
#include <esp_timer.h>
#include "config.h"
#include "midi_transport.h"

namespace {
struct DebouncedInput {
  bool stable = false;
  bool candidate = false;
  uint32_t since = 0;
  void sample(bool value, uint32_t now) {
    if (value != candidate) {
      candidate = value;
      since = now;
    }
    if (now - since >= Config::DEBOUNCE_MS) stable = candidate;
  }
};
DebouncedInput keys[12];
DebouncedInput button;
uint8_t controller = Config::ENCODER_INITIAL;
portMUX_TYPE encoderMux = portMUX_INITIALIZER_UNLOCKED;
volatile int32_t encoderSteps = 0;
volatile uint8_t previousAB = 0;
volatile int accumulator = 0;
TaskHandle_t inputTask = nullptr;
esp_timer_handle_t scanTimer = nullptr;

void scanTick(void*) {
  xTaskNotifyGive(inputTask);
}
// Stored in DRAM: interrupt may run while the flash cache is disabled.
DRAM_ATTR const int8_t transitions[] = {0, -1, 1, 0, 1, 0, 0, -1,
                                       -1, 0, 0, 1, 0, 1, -1, 0};

void ARDUINO_ISR_ATTR encoderInterrupt() {
  portENTER_CRITICAL_ISR(&encoderMux);
  const uint8_t ab = (digitalRead(Config::ENCODER_A) << 1) | digitalRead(Config::ENCODER_B);
  if ((previousAB ^ ab) == 3) accumulator = 0;
  else accumulator = accumulator + transitions[(previousAB << 2) | ab];
  previousAB = ab;
  if (accumulator >= Config::ENCODER_TRANSITIONS) {
    if (encoderSteps < 1024) encoderSteps = encoderSteps + 1;
    accumulator = 0;
  } else if (accumulator <= -Config::ENCODER_TRANSITIONS) {
    if (encoderSteps > -1024) encoderSteps = encoderSteps - 1;
    accumulator = 0;
  }
  portEXIT_CRITICAL_ISR(&encoderMux);
}

void scanInputs() {
  static uint32_t lastScan = 0;
  const uint32_t nowUs = micros();
  if (nowUs - lastScan < Config::SCAN_INTERVAL_US) return;
  lastScan = nowUs;
  const uint32_t now = millis();
  for (unsigned row = 0; row < 4; ++row) {
    digitalWrite(Config::ROW_PINS[row], LOW);
    delayMicroseconds(3);
    for (unsigned col = 0; col < 3; ++col) {
      keys[row * 3 + col].sample(digitalRead(Config::COL_PINS[col]) == LOW, now);
    }
    // Open-drain HIGH releases the row; never drive an inactive row HIGH.
    digitalWrite(Config::ROW_PINS[row], HIGH);
  }
  button.sample(digitalRead(Config::ENCODER_BUTTON) == LOW, now);
}
}  // namespace

void setup() {
  for (auto pin : Config::ROW_PINS) {
    digitalWrite(pin, HIGH);
    pinMode(pin, OUTPUT_OPEN_DRAIN);
  }
  for (auto pin : Config::COL_PINS) pinMode(pin, INPUT_PULLUP);
  pinMode(Config::ENCODER_A, INPUT_PULLUP);
  pinMode(Config::ENCODER_B, INPUT_PULLUP);
  pinMode(Config::ENCODER_BUTTON, INPUT_PULLUP);
  pinMode(Config::LED, OUTPUT);
  digitalWrite(Config::LED, !Config::LED_ACTIVE_HIGH);
  previousAB = (digitalRead(Config::ENCODER_A) << 1) | digitalRead(Config::ENCODER_B);
  attachInterrupt(Config::ENCODER_A, encoderInterrupt, CHANGE);
  attachInterrupt(Config::ENCODER_B, encoderInterrupt, CHANGE);
  Serial.begin(115200);
  midiBegin();
  Serial.println("ChainOSCPad MIDI ready");
  inputTask = xTaskGetCurrentTaskHandle();
  esp_timer_create_args_t timerArgs{};
  timerArgs.callback = scanTick;
  timerArgs.name = "midi-scan";
  timerArgs.skip_unhandled_events = true;
  ESP_ERROR_CHECK(esp_timer_create(&timerArgs, &scanTimer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(scanTimer, Config::SCAN_INTERVAL_US));
}

void loop() {
  // Block between 1 ms timer notifications, independently of the RTOS tick.
  // Staying here also avoids Arduino's periodic 5-tick unicore loop delay.
  // The blocked task leaves the CPU available to BLE, USB and the idle task.
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    scanInputs();
    portENTER_CRITICAL(&encoderMux);
    const int32_t steps = encoderSteps;
    encoderSteps = 0;
    portEXIT_CRITICAL(&encoderMux);
    const int value = controller + steps * Config::ENCODER_DIRECTION * Config::ENCODER_STEP;
    controller = constrain(value, 0, 127);
    uint16_t pressed = 0;
    for (unsigned i = 0; i < 12; ++i) if (keys[i].stable) pressed |= 1u << i;
    midiUpdate(pressed, button.stable, controller);
    const uint32_t now = millis();
    const bool ledOn = midiConnected() ? (now - midiLastActivity() >= 40) : ((now % 1000) < 500);
    digitalWrite(Config::LED, ledOn == Config::LED_ACTIVE_HIGH ? HIGH : LOW);
  }
}
