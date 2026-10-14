#pragma once
#include "Hal.hpp"

namespace DisplayLayout {
constexpr uint8_t columns = 128;
constexpr uint8_t rows = 8;
constexpr uint16_t sensorAngleDegrees = 180; // Primary points left.
constexpr uint16_t stripSeparationDegrees = 180;
constexpr bool clockwise = true;
constexpr uint8_t stripPhase(Strip strip) {
    return strip == Strip::Second ? columns / 2 : 0;
}
// Font rows run top-to-bottom; PA0 is inner and PA7 is outer.
constexpr uint8_t fontRowMask(uint8_t row) {
    return row < rows ? uint8_t(1u << (rows - 1 - row)) : 0;
}
}