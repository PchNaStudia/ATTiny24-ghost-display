#include "Hal.hpp"

int main() {
    Hal::initialize();
    // C runtime initialization establishes controller state; initialize()
    // leaves the display blank and column events disabled.
    Hal::enableEvents();
    while (true) Hal::idle();
}
