#pragma once
#include <stdint.h>

namespace DisplayController {
void reset();
void onRotation();
void onColumnDeadline();
void onTimerOverflow();
}