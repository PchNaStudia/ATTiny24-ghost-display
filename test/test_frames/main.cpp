#include "virtual_frames.hpp"
#include <cstdlib>
#include <iostream>

static_assert(makeHalfFrame("TEST").fits, "short half frame");
static_assert(sizeof(HalfFrame::columns) == HALF, "half frame stores half a circle");
static_assert(!makeTwoHalves(makeHalfFrame("TEST"), makeHalfFrame("WWWWWWW")).fits,
              "lower half overflow is validated");
static_assert(!makeTwoHalves(makeHalfFrame("WWWWWWW"), makeHalfFrame("TEST")).fits,
              "upper half overflow is validated");
static_assert(!makeHalfFrame("WWWWWWW").fits, "half overflow");
static_assert(makeFrame("WWWW").fits, "full circle capacity");
static_assert(!makeFrame("WWWWWWWWWWWWWW").fits, "full circle overflow");
static_assert(ledfont::decode("\\{65536}\\").glyph == -1, "overflow rejected");
static_assert(ledfont::decode("\\{655360}\\").glyph == -1, "overflow stays invalid");

int main() {
    constexpr auto left = makeHalfFrame("AI", TextAlignment::Left);
    constexpr auto centered = makeHalfFrame("AI");
    constexpr auto right = makeHalfFrame("AI", TextAlignment::Right);
    constexpr int used = ledfont::drawText("AI", 0, 0, [](int,int,bool) constexpr {}) - 1;
    for (uint8_t col = 0; col < HALF; ++col) {
        const int centerSource = int(col) - (HALF - used) / 2;
        const int rightSource = int(col) - (HALF - used);
        if (centered.columns[col] != (centerSource >= 0 ? left.columns[centerSource] : 0) ||
            right.columns[col] != (rightSource >= 0 ? left.columns[rightSource] : 0)) return EXIT_FAILURE;
    }
    const AnimationEntry reused[] = {
        virtualFrame<0>(1, Animation::None, Strip::First),
        virtualFrame<0>(1, Animation::None, Strip::Second)
    };
    for (uint8_t col = 0; col < COLS; ++col)
        if (getColumn(1, col, reused, 2) != getColumn(0, (col + HALF) % COLS, reused, 2))
            return EXIT_FAILURE;
    // Verify font top/bottom against physical pins, independent of frame mapping.
    constexpr Frame glyph = makeFrame("A");
    for (uint8_t x = 0; x < ledfont::widths[0]; ++x)
        if (glyph.columns[x] != reverseBits(ledfont::bitmap[x])) return EXIT_FAILURE;
    if (DisplayLayout::fontRowMask(0) != 0x80 ||
        DisplayLayout::fontRowMask(7) != 0x01 ||
        DisplayLayout::sensorAngleDegrees != 180 ||
        DisplayLayout::stripSeparationDegrees != 180) return EXIT_FAILURE;
    Frame upper{};
    upper.displayMode = DisplayMode::TwoHalves;
    upper.columns[0] = 0x80;
    upper.columns[HALF - 1] = 0x01;
    upper.columns[HALF] = 0x04;
    upper.columns[COLS - 1] = 0x02;
    if (renderColumn(&upper, 0) != 0x80 ||
        renderColumn(&upper, HALF - 1) != 0x01 ||
        renderColumn(&upper, HALF) != 0x40 ||
        renderColumn(&upper, COLS - 1) != 0x20) return EXIT_FAILURE;
    Frame secondary = upper;

    for (uint8_t col = 0; col < COLS; ++col)
        if (renderColumn(&secondary, col, Strip::Second) != renderColumn(&upper, (col + HALF) % COLS))
            return EXIT_FAILURE;
    for (unsigned b = 0; b < 256; ++b) {
        if (reverseBits(reverseBits(uint8_t(b))) != b) return EXIT_FAILURE;
    }
    if (getColumn(VIRTUAL_FRAME_COUNT, 0) != 0) return EXIT_FAILURE;
    if (nextVirtualFrame(VIRTUAL_FRAME_COUNT - 1) != 0) return EXIT_FAILURE;
    VirtualFrameIndex position = VIRTUAL_FRAME_COUNT - 1;
    for (unsigned cycle = 0; cycle < 3; ++cycle) {
        for (uint16_t expected = 0; expected < VIRTUAL_FRAME_COUNT; ++expected) {
            position = nextVirtualFrame(position);
            if (position != expected) return EXIT_FAILURE;
            uint16_t local = expected;
            const AnimationEntry* selected = nullptr;
            for (const auto& entry : animation) {
                if (local < entry.repetitions) { selected = &entry; break; }
                local -= entry.repetitions;
            }
            const uint8_t physical = selected->frameIndex;
            if (virtual_frames_detail::resolve(position) != &frames[physical] ||
                getStrip(position) != selected->strip) return EXIT_FAILURE;
        }
    }
    const AnimationEntry schedule[] = {
        virtualFrame<1>(1), virtualFrame<0>(2),
        virtualFrame<1>(600), virtualFrame<0>(1)
    };
    for (uint16_t i = 0; i < 604; ++i) {
        const uint8_t physical = i == 0 ? 1 : i < 3 ? 0 : i < 603 ? 1 : 0;
        const Frame* frame = virtual_frames_detail::resolve(i, schedule, 4);
        if (frame != &frames[physical]) return EXIT_FAILURE;
        for (uint8_t col = 0; col < COLS; ++col)
            if (renderColumn(frame, col) != renderColumn(&frames[physical], col))
                return EXIT_FAILURE;
    }
    const AnimationEntry maximum[] = {virtualFrame<0>(65535)};
    if (virtual_frames_detail::resolve(65534, maximum, 1) != &frames[0] ||
        virtual_frames_detail::resolve(65535, maximum, 1) != nullptr)
        return EXIT_FAILURE;
    const AnimationEntry invalid[] = {{1, FRAME_COUNT}};
    if (virtual_frames_detail::resolve(604, schedule, 4) != nullptr ||
        virtual_frames_detail::resolve(0, invalid, 1) != nullptr ||
        renderColumn(nullptr, 0) != 0) return EXIT_FAILURE;
    std::cout << "PASS: frame capacity, virtual timeline, long repetitions, mapping and bit reversal\n";
}
