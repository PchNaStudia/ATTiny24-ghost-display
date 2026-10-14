#include "Hal.hpp"
#include "DisplayController.hpp"
#include "virtual_frames.hpp"
#include <cstdlib>
#include <iostream>

namespace {
uint32_t now = 0;
uint16_t deadline = 0;
uint8_t output = 0;
Strip selected = Strip::First;
bool scheduled = false;
bool reject = false;
void check(bool condition, const char* message) {
    if (!condition) {
        std::cout << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
void rotation(uint32_t time) {
    now = time;
    DisplayController::onRotation();
}
void column() {
    check(scheduled, "column event must be scheduled");
    now += uint16_t(deadline - uint16_t(now));
    DisplayController::onColumnDeadline();
}
}

namespace Hal {
void initialize() { now = 0; scheduled = false; output = 0; reject = false; }
void enableEvents() {}
void idle() {}
uint32_t timestamp() { return now; }
void selectStrip(Strip strip) { selected = strip; }
void writeColumn(uint8_t bits) { output = bits; }
void cancelColumn() { scheduled = false; }
bool scheduleColumn(uint16_t next) {
    const uint16_t remaining = next - uint16_t(now);
    scheduled = !reject && remaining >= 8 && remaining <= 32767;
    deadline = next;
    return scheduled;
}
}

int main() {
    Hal::initialize();
    // Firmware startup relies on static initialization, not reset().
    rotation(100);
    check(!scheduled && output == 0, "cold startup first pulse synchronizes");
    rotation(6500);
    check(scheduled && output == getColumn(0, 0), "cold startup starts first frame");
    DisplayController::reset();
    check(!scheduled && output == 0, "reset blanks display");
    rotation(100);
    check(!scheduled, "first pulse only synchronizes");
    rotation(6500);
    check(scheduled && deadline == 6500 + 6400 / COLS && selected == Strip::First,
          "second pulse starts first frame");
    check(output == getColumn(0, 0), "first column");
    rotation(6501);
    check(deadline == 6500 + 6400 / COLS, "bounce ignored");
    for (uint8_t i = 1; i < COLS; ++i) {
        column();
        check(output == getColumn(0, i), "entire rendered rotation");
    }
    column();
    check(!scheduled && output == 0, "end of rotation blanks display");
    for (uint16_t frame = 1; frame <= VIRTUAL_FRAME_COUNT; ++frame) {
        rotation(6500 + uint32_t(frame) * 6400);
        const VirtualFrameIndex index = frame % VIRTUAL_FRAME_COUNT;
        check(selected == getStrip(index) && output == getColumn(index, 0),
              "timeline, strip selection and wrap");
    }
    now += 62501;
    DisplayController::onTimerOverflow();
    check(!scheduled && output == 0, "timeout blanks display");
    rotation(now + 100);
    check(!scheduled, "timeout requires synchronization pulse");
    rotation(now + 6400);
    check(scheduled, "reacquisition");
    reject = true;
    column();
    check(!scheduled && output == 0, "missed deadline fails safely");
    reject = false;
    DisplayController::reset();
    rotation(0xFFFFFF00u);
    rotation(uint32_t(0xFFFFFF00u + 6400u));
    check(scheduled, "32-bit timestamp rollover");
    column();
    check(output == getColumn(0, 1), "column after rollover");
    DisplayController::reset();
    rotation(100);
    rotation(6500);
    rotation(70000);
    check(!scheduled && output == 0, "long rotation resynchronizes");
    std::cout << "PASS: portable controller with mock HAL, columns, bounce, timeline, timeout, missed deadlines and rollover\n";
}