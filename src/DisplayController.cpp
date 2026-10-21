#include "DisplayController.hpp"
#include "Hal.hpp"
#include "prepared_animation.hpp"

namespace {
constexpr uint16_t MIN_COLUMN_TICKS = 32;
constexpr uint32_t MAX_ROTATION_TICKS = 62500;
uint32_t lastRotation = 0;
uint16_t columnTicks = 0;
uint16_t deadline = 0;
uint8_t currentColumn = 0;
VirtualFrameIndex activeVirtualFrame = VIRTUAL_FRAME_COUNT - 1;
PreparedAnimation prepared{};
bool synchronized = false;

void stopDisplay() {
    Hal::writeColumn(0);
    Hal::cancelColumn();
}
void schedule(uint16_t next) {
    deadline = next;
    // A rejected deadline already disables column events.
    if (!Hal::scheduleColumn(next)) Hal::writeColumn(0);
}
}

namespace DisplayController {
void reset() {
    lastRotation = 0;
    columnTicks = deadline = 0;
    currentColumn = 0;
    activeVirtualFrame = VIRTUAL_FRAME_COUNT - 1;
    synchronized = false;
    stopDisplay();
}
void onColumnDeadline() {
    if (currentColumn >= COLS) {
        stopDisplay();
        return;
    }
    Hal::writeColumn(prepared.column(currentColumn));
    ++currentColumn;
    schedule(deadline + columnTicks);
}
void onTimerOverflow() {
    if (synchronized && Hal::timestamp() - lastRotation > MAX_ROTATION_TICKS) {
        synchronized = false;
        stopDisplay();
    }
}
void onRotation() {
    const uint32_t now = Hal::timestamp();
    const uint32_t elapsed = now - lastRotation;
    if (synchronized && elapsed < COLS * MIN_COLUMN_TICKS) return;
    lastRotation = now;
    if (!synchronized || elapsed > MAX_ROTATION_TICKS) {
        synchronized = true;
        stopDisplay();
        return;
    }
    columnTicks = elapsed / COLS;
    Hal::cancelColumn();
    activeVirtualFrame = nextVirtualFrame(activeVirtualFrame);
    Hal::selectStrip(prepared.prepare(activeVirtualFrame));
    Hal::writeColumn(prepared.column(0));
    currentColumn = 1;
    schedule(uint16_t(now) + columnTicks);
}
}