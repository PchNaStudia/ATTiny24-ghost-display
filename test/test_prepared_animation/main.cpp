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
    // Independent regression: alignment must rotate the entire animated result,
    // not split a glyph at sensor zero. Exercise all operations and combinations.
    const auto original = makeFrame("HELLO WORLD");
    for (FrameAlignment alignment : {FrameAlignment::Top, FrameAlignment::Left,
                                    FrameAlignment::Bottom, FrameAlignment::Right}) {
        const auto aligned = makeFrame("HELLO WORLD", alignment);
        for (unsigned bits = 0; bits < 128; ++bits) {
            const auto effect = static_cast<Animation>(bits);
            if (!validAnimation(effect)) continue;
            for (Strip strip : {Strip::First, Strip::Second}) {
                for (uint16_t step = 0; step < 129; ++step) {
                    AnimationState expected{}, actual{};
                    expected.prepare(&original, effect, step, 129, strip);
                    actual.prepare(&aligned, effect, step, 129, strip);
                    for (uint8_t col = 0; col < COLS; ++col) {
                        const uint8_t local = (col + COLS - aligned.origin) & (COLS - 1);
                        if (actual.column(col) != expected.column(local)) return EXIT_FAILURE;
                        if (effect == Animation::None && actual.column(col) != renderColumn(&aligned, col, strip))
                            return EXIT_FAILURE;
                    }
                }
            }
        }
    }
    // Exercise every valid effect encoding, both layouts/strips, and endpoints.
    for (const auto& frame : frames) for (unsigned flags = 0; flags < 128; ++flags) {
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
