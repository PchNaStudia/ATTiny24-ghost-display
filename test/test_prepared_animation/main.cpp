#include "prepared_animation.hpp"
#include <cstdlib>
#include <iostream>

int main() {
    PreparedAnimation prepared{};
    for (uint16_t position = 0; position < VIRTUAL_FRAME_COUNT; ++position) {
        if (prepared.prepare(position) != getStrip(position)) return EXIT_FAILURE;
        for (uint8_t column = 0; column < COLS; ++column)
            if (prepared.column(column) != getColumn(position, column)) return EXIT_FAILURE;
    }
    // Exercise every valid effect encoding, both layouts/strips, and endpoints.
    for (const auto& frame : frames) for (unsigned flags = 0; flags < 32; ++flags) {
        const Animation effect = static_cast<Animation>(flags);
        if (!validAnimation(effect)) continue;
        for (Strip strip : {Strip::First, Strip::Second}) {
            for (uint16_t duration : {1, 2, 32, 128, 255, 600}) {
                for (uint16_t step = 0; step < duration; ++step) {
                    prepared.prepare<0xFFFF>(&frame, effect, step, duration, strip);
                    for (uint8_t column = 0; column < COLS; ++column)
                        if (prepared.column(column) != animatedColumn(&frame, column, effect, step, duration, strip)) {
                            std::cerr << "Mismatch: " << flags << ' ' << duration << ' '
                                      << step << ' ' << unsigned(column) << '\n';
                            return EXIT_FAILURE;
                        }
                }
            }
        }
    }
    std::cout << "PASS: prepared rendering matches timeline and all valid effects/layouts/strips\n";
}
