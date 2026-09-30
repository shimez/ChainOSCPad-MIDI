#include <Arduino.h>
#include <NimBLEDevice.h>
#include <atomic>
#include "config.h"
#include "midi_transport.h"

#if PAD_USB_MIDI
#include <USB.h>
#include <USBMIDI.h>
#include <tusb.h>
static USBMIDI usbMidi;
#endif

namespace {
constexpr char SERVICE_UUID[] = "03B80E5A-EDE8-4B33-A751-6CE34EC4C700";
constexpr char MIDI_UUID[] = "7772E5DB-3868-4112-A1A9-F2669D106BF3";
NimBLECharacteristic* characteristic = nullptr;
std::atomic<bool> subscribed{false};
std::atomic<uint32_t> generation{0};
uint32_t activity = 0;
bool connected = false;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* server, NimBLEConnInfo& info) override {
    server->updateConnParams(info.getConnHandle(), 6, 12, 0, 400);
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
    subscribed.store(false);
    generation.fetch_add(1);
  }
} serverCallbacks;

class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t value) override {
    subscribed.store((value & 1) != 0);
    generation.fetch_add(1);
  }
} characteristicCallbacks;

struct State {
  uint16_t keys = 0;
  int sustain = -1;
  int controller = -1;
  uint8_t resetStep = 0;
};
State bleState;
#if PAD_USB_MIDI
State usbState;
#endif

bool send(bool usb, uint8_t status, uint8_t data1, uint8_t data2) {
  status |= Config::CHANNEL - 1;
  bool sent = false;
  if (usb) {
#if PAD_USB_MIDI
    midiEventPacket_t packet{static_cast<uint8_t>(status >> 4), status, data1, data2};
    sent = usbMidi.writePacket(&packet);
#endif
  } else if (subscribed.load()) {
    const uint16_t timestamp = millis() & 0x1fff;
    const uint8_t packet[] = {
        static_cast<uint8_t>(0x80 | (timestamp >> 7)),
        static_cast<uint8_t>(0x80 | (timestamp & 0x7f)), status, data1, data2};
    sent = characteristic->notify(packet, sizeof(packet));
  }
  if (sent) activity = millis();
  return sent;
}

void update(State& state, bool ready, bool usb, uint16_t keys, bool sustain,
            uint8_t controller) {
  if (!ready) {
    state = State{};
    return;
  }
  // Clear stale sustain/voices after enumeration or BLE re-subscription.
  const uint8_t resetCC[] = {64, 120, 123};
  while (state.resetStep < sizeof(resetCC)) {
    if (!send(usb, 0xb0, resetCC[state.resetStep], 0)) return;
    ++state.resetStep;
  }
  const int buttonValue = sustain ? 127 : 0;
  if (state.sustain != buttonValue) {
    if (!send(usb, 0xb0, Config::BUTTON_CC, buttonValue)) return;
    state.sustain = buttonValue;
  }
  for (unsigned i = 0; i < 12; ++i) {
    const uint16_t mask = 1u << i;
    if ((state.keys & mask) == (keys & mask)) continue;
    const bool pressed = (keys & mask) != 0;
    if (!send(usb, pressed ? 0x90 : 0x80, Config::NOTES[i],
              pressed ? Config::VELOCITY : 0)) return;
    state.keys ^= mask;
  }
  if (state.controller != controller && send(usb, 0xb0, Config::ENCODER_CC, controller)) {
    state.controller = controller;
  }
}
}  // namespace

void midiBegin() {
#if PAD_USB_MIDI
  USB.productName(Config::DEVICE_NAME);
  usbMidi.begin();
  USB.begin();
#endif
  NimBLEDevice::init(Config::DEVICE_NAME);
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  auto* server = NimBLEDevice::createServer();
  server->setCallbacks(&serverCallbacks, false);
  server->advertiseOnDisconnect(true);
  auto* service = server->createService(SERVICE_UUID);
  characteristic = service->createCharacteristic(
      MIDI_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY);
  characteristic->setCallbacks(&characteristicCallbacks);
  const uint8_t empty[] = {0x80, 0x80};
  characteristic->setValue(empty, sizeof(empty));
  server->start();
  auto* advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->enableScanResponse(true);
  advertising->setName(Config::DEVICE_NAME);
  advertising->start();
}

void midiUpdate(uint16_t keys, bool sustain, uint8_t controller) {
  static uint32_t lastGeneration = 0;
  const uint32_t current = generation.load();
  if (current != lastGeneration) {
    bleState = State{};
    lastGeneration = current;
  }
  const bool bleReady = subscribed.load();
  update(bleState, bleReady, false, keys, sustain, controller);
  connected = bleReady;
#if PAD_USB_MIDI
  const bool usbReady = tud_midi_mounted() && !tud_suspended();
  update(usbState, usbReady, true, keys, sustain, controller);
  connected |= usbReady;
  // This is a controller, but drain host traffic so OUT cannot fill up.
  midiEventPacket_t packet;
  for (unsigned i = 0; i < 16 && usbMidi.readPacket(&packet); ++i) {}
#endif
}

bool midiConnected() { return connected; }
uint32_t midiLastActivity() { return activity; }
