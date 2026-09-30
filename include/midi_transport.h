#pragma once
#include <stdint.h>

void midiBegin();
// Called from the loop only. Each transport independently retries unsent state.
void midiUpdate(uint16_t keys, bool sustain, uint8_t controller);
bool midiConnected();
uint32_t midiLastActivity();
