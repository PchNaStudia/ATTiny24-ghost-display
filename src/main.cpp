#include "Errors.h"
#include "font.hpp"

int setup();
int loop();

void entry() {
    auto setupResult = setup();
    if (setupResult != 0) {
        if (reportError(setupResult)){
            return;
        }
    }
    while (true) {
        auto loopResult = loop();
        if (reportError(loopResult)){
            return;
        }
    }
}

int main() {
    entry();
    // on AVR main should never terminate!
    while (true) {
    }
}

int setup(){return 0;}
int loop(){return 0;}
