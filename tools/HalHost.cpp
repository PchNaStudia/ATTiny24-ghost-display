#include "Hal.hpp"
#include "DisplayController.hpp"
#include "DisplayLayout.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace {
uint64_t now = 0, nextOverflow = 65536, nextColumn = 0;
uint64_t nextRotation = 0;
uint32_t period = 6250;
unsigned strip = 0;
bool scheduled = false;
bool enabled = false;
void advance(uint32_t ticks) {
    const uint64_t target = now + ticks;
    while (true) {
        uint64_t event = std::min(nextRotation, nextOverflow);
        if (scheduled) event = std::min(event, nextColumn);
        if (event > target) break;
        now = event;
        // Compare events precede rotation pulses when deadlines coincide.
        if (scheduled && nextColumn == now) {
            scheduled = false;
            DisplayController::onColumnDeadline();
        }
        if (nextOverflow == now) {
            nextOverflow += 65536;
            DisplayController::onTimerOverflow();
        }
        if (nextRotation == now) {
            nextRotation += period;
            std::cout << "{\"type\":\"rotation\",\"tick\":" << now
                      << ",\"period\":" << period
                      << ",\"columns\":" << unsigned(DisplayLayout::columns)
                      << ",\"sensorAngleDegrees\":" << DisplayLayout::sensorAngleDegrees
                      << ",\"stripSeparationDegrees\":" << DisplayLayout::stripSeparationDegrees
                      << ",\"clockwise\":" << (DisplayLayout::clockwise ? "true" : "false") << "}\n";
            DisplayController::onRotation();
        }
    }
    now = target;
    std::cout.flush();
}
}
namespace Hal {
void initialize() {
    std::cout << "{\"type\":\"ready\"}\n" << std::flush;
}
void enableEvents() { enabled = true; }
void idle() {
    std::string line;
    if (!std::getline(std::cin, line)) std::exit(0);
    std::istringstream command(line);
    std::string name;
    uint32_t value = 0;
    if (!(command >> name >> value)) return;
    if (name == "column" && enabled) {
        const uint64_t target = scheduled ? nextColumn : nextRotation;
        advance(uint32_t(target - now));
    }
    if (name == "frame" && enabled) advance(uint32_t(nextRotation - now));
    if (name == "advance" && enabled && value <= 125000) advance(value);
    if (name == "period" && value >= 2048 && value <= 62500) {
        period = value;
        nextRotation = now + period;
    }
}
uint32_t timestamp() { return uint32_t(now); }
void selectStrip(Strip value) { strip = unsigned(value); }
void writeColumn(uint8_t bits) {
    std::cout << "{\"type\":\"write\",\"tick\":" << now
              << ",\"strip\":" << strip << ",\"bits\":" << unsigned(bits) << "}\n";
}
void cancelColumn() { scheduled = false; }
bool scheduleColumn(uint16_t deadline) {
    const uint16_t remaining = deadline - uint16_t(now);
    scheduled = remaining >= 8 && remaining <= 32767;
    nextColumn = now + remaining;
    return scheduled;
}
}