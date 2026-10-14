#pragma once
#include <stdint.h>

// Storage annotation only; no platform headers or platform-specific types escape.
#ifdef __AVR__
#define HAL_STORAGE __attribute__((__progmem__))
#else
#define HAL_STORAGE
#endif

enum class Strip : uint8_t { First = 0, Second = 1 };

namespace Hal {
// Timer ticks are CPU clock / 64 (8 us at the configured 8 MHz).
// Events are serialized: a host adapter must dispatch them on one thread.
void initialize();
void enableEvents();
void idle();
uint32_t timestamp();
void selectStrip(Strip strip);
// Bit n drives PAn: bit 0 is innermost, bit 7 outermost on either strip.
void writeColumn(uint8_t bits);
void cancelColumn();
// Absolute low 16-bit deadline. Rejects deadlines less than 8 ticks away
// or more than 32767 ticks away, including missed deadlines.
// Rejection disables column events before returning false.
bool scheduleColumn(uint16_t deadline);

#ifdef __AVR__
uint8_t readByte(const void* address);
uint16_t readWord(const void* address);
#else
inline uint8_t readByte(const void* address) {
    return *static_cast<const uint8_t*>(address);
}
inline uint16_t readWord(const void* address) {
    return *static_cast<const uint16_t*>(address);
}
#endif
}