#include "virtual_frames.hpp"
#include <cstdlib>
#include <iostream>

static void check(bool condition, const char* message) {
    if (!condition) {
        std::cout << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main() {
    for (uint8_t width : {HALF, COLS})
        for (bool out : {false, true})
            for (uint16_t progress = 0; progress < 256; ++progress) {
                const uint8_t amount = out ? uint8_t(progress) : uint8_t(255 - progress);
                const uint8_t expected = amount == 0 ? 0 :
                    (uint16_t(amount + 1) * width) >> 8;
                check(animation_detail::distance(uint8_t(progress), width, out) == expected,
                      "distance shifts preserve all stages and endpoints");
            }
    constexpr Frame text = makeFrame("AI");
    Frame pattern{};
    for (uint8_t col = 0; col < COLS; ++col) pattern.columns[col] = col + 1;
    const Animation ins[] = {Animation::WipeIn,
        Animation::WalkIn, Animation::WalkInReverse, Animation::WipeInReverse, Animation::Typewriter};
    const Animation outs[] = {Animation::WipeOut, Animation::WalkOut, Animation::WalkOutReverse, Animation::WipeOutReverse, Animation::Backspace, Animation::ForwardDelete};
    for (Animation effect : ins)
        for (uint8_t col = 0; col < COLS; ++col) {
            check(animatedColumn(&pattern, col, effect, 0, 9) == 0, "in starts blank");
            check(animatedColumn(&pattern, col, effect, 8, 9) == pattern.columns[col], "in ends full");
            check(animatedColumn(&pattern, col, effect, 0, 1) == pattern.columns[col], "one-step in ends full");
        }
    for (Animation effect : outs)
        for (uint8_t col = 0; col < COLS; ++col) {
            check(animatedColumn(&pattern, col, effect, 0, 9) == pattern.columns[col], "out starts full");
            check(animatedColumn(&pattern, col, effect, 8, 9) == 0, "out ends blank");
            check(animatedColumn(&pattern, col, effect, 0, 1) == 0, "one-step out ends blank");
        }
    for (uint8_t col = 0; col < COLS; ++col) {
        const uint8_t left = col < HALF ? pattern.columns[col] : 0;
        const uint8_t right = col >= HALF ? pattern.columns[col] : 0;
        check(animatedColumn(&pattern, col, Animation::WipeIn, 1, 3)== left, "wipe in middle");
        check(animatedColumn(&pattern, col, Animation::WipeOut, 1, 3)== right, "wipe out middle");
        const uint8_t slideRight = col >= HALF ? pattern.columns[col - HALF] : 0;
        const uint8_t slideLeft = col < HALF ? pattern.columns[col + HALF] : 0;
        check(animatedColumn(&pattern, col, Animation::WalkIn, 1, 3)== slideRight, "walk in shift");
        check(animatedColumn(&pattern, col, Animation::WalkOutReverse, 1, 3)== slideRight, "reverse out shift");
        check(animatedColumn(&pattern, col, Animation::WalkOut, 1, 3)== slideLeft, "walk out shift");
        check(animatedColumn(&pattern, col, Animation::WalkInReverse, 1, 3)== slideLeft, "reverse in shift");
    }
    // Shared stage calculation is exact across the full 16-bit duration range.
    for (uint16_t duration : {uint16_t(1), uint16_t(2), uint16_t(65), uint16_t(65535)})
        for (uint32_t step = 0; step < duration; ++step)
            check(animation_detail::stage(uint16_t(step), duration) ==
                (duration == 1 ? 255 : step * 255 / (duration - 1)), "exact stage");
    for (uint16_t sample = 0; sample < 256; ++sample) {
        check(!animation_detail::blinkBlank(0, sample), "blink start full");
        check(!animation_detail::blinkBlank(255, sample), "blink end full");
    }
    unsigned blankStages = 0, transitions = 0;
    bool previous = false;
    for (uint16_t stage = 0; stage < 256; ++stage) {
        const bool blank = animation_detail::blinkBlank(uint8_t(stage), 0);
        blankStages += blank;
        transitions += blank != previous;
        previous = blank;
        check(blank == animation_detail::blinkBlank(uint8_t(stage), 65535),
              "blink is independent of pseudo-random step sampling");
    }
    check(blankStages == 64 && transitions == 8, "four regular blinks with 75 percent visible duty");
    Frame spaced{};
    for (uint8_t col = 0; col < 5; ++col) spaced.columns[col] = 255;
    for (uint8_t col = 6; col < 10; ++col) spaced.columns[col] = 255;
    check(animation_detail::roundToEmpty(&spaced, 2, COLS) == 5, "snap to spacer");
    check(animation_detail::roundToEmpty(&spaced, 7, COLS) == 10, "snap next glyph");
    check(animation_detail::roundToEmpty(&pattern, 1, COLS) == COLS, "snap to end");
    for (uint16_t step = 0; step < 256; ++step)
        for (uint8_t col = 0; col < COLS; ++col)
            check(animatedColumn(&spaced, col, Animation::OneByOne, step, 256) ==
                ((step == 0 ? false : step <= 10 ? col < 5 : col >= 6 && col < 10)
                    ? spaced.columns[col] : 0), "one character at original position");
    Frame empty{};
    // Different character counts and offsets must be processed independently.
    HalfFrame top{}, bottom{};
    top.columns[0] = 0x80;
    bottom.columns[2] = 0x04;
    bottom.columns[10] = 0x02;
    Frame pair = makeTwoHalves(top, bottom);
    for (Strip strip : {Strip::First, Strip::Second}) {
        const uint8_t phase = DisplayLayout::stripPhase(strip);
        for (uint8_t col = 0; col < COLS; ++col) {
            const uint8_t p = (col + phase) % COLS;
            const uint8_t local = p < HALF ? p : COLS - 1 - p;
            uint8_t expected = p < HALF ? top.columns[local]
                : (local == 10 ? bottom.columns[local] : 0);
            if (p >= HALF) expected = reverseBits(expected);
            check(animatedColumn(&pair, col, Animation::OneByOne, 1, 2, strip)== expected,
                  "each half selects its own last character");
            check(animatedColumn(&pair, col, Animation::WalkIn, 8, 9, strip)== renderColumn(&pair, col, strip),
                  "independent half walk endpoint");
            check(animatedColumn(&pair, col, Animation::WalkOut, 8, 9, strip)== 0,
                  "half walk cannot bleed across boundary");
        }
    }
    for (uint8_t col = 0; col < COLS; ++col) {
        check(animatedColumn(&empty, col, Animation::OneByOne, 0, 1) == 0,
              "one-by-one empty text");
        check(animatedColumn(&spaced, col, Animation::OneByOne, 0, 1)==
            (col >= 6 && col < 10 ? spaced.columns[col] : 0), "one-step last character");
    }
    for (Strip strip : {Strip::First, Strip::Second}) {
        spaced = makeTwoHalves(makeHalfFrame(""), makeHalfFrame(""));
        for (uint8_t col = 0; col < 10; ++col) {
            const uint8_t bits = col < 5 || col >= 6 ? 255 : 0;
            spaced.columns[col] = spaced.columns[HALF + col] = bits;
        }
        for (uint8_t col = 0; col < COLS; ++col) {
            const uint8_t p = (col + (strip == Strip::Second ? HALF : 0)) % COLS;
            const uint8_t logical = p < HALF ? p : COLS - 1 - p;
            uint8_t expected = logical >= 6 && logical < 10 ? spaced.columns[logical] : 0;
            if (p >= HALF) expected = reverseBits(expected);
            check(animatedColumn(&spaced, col, Animation::OneByOne, 7, 8, strip)== expected,
                  "one-by-one strip and half mapping");
        }
    }
    spaced.displayMode = DisplayMode::FullCircle;
    for (uint8_t col = HALF; col < COLS; ++col) spaced.columns[col] = 0;
    for (uint8_t col = 0; col < COLS; ++col) {
        check(animatedColumn(&spaced, col, Animation::Typewriter, 1, COLS / 4 + 1) ==
            (col < 5 ? spaced.columns[col] : 0), "typewriter snaps cutoff");
        check(animatedColumn(&spaced, col, Animation::Backspace, COLS / 4 - 1, COLS / 4 + 1) ==
            (col < 5 ? spaced.columns[col] : 0), "backspace snaps cutoff");
    }
    // Transform logical content first, then apply physical strip/half mapping.
    for (Strip strip : {Strip::First, Strip::Second}) {
        pattern.displayMode = DisplayMode::TwoHalves;
        for (uint8_t col = 0; col < COLS; ++col) {
            const uint8_t p = (col + (strip == Strip::Second ? HALF : 0)) % COLS;
            const uint8_t logical = p < HALF ? p : COLS - 1 - p;
            uint8_t expected = logical < HALF / 2 ? pattern.columns[(p < HALF ? 0 : HALF) + logical] : 0;
            if (p >= HALF) expected = reverseBits(expected);
            check(animatedColumn(&pattern, col, Animation::WipeIn, 1, 3, strip)== expected, "half/strip transform order");
        }
    }
    const AnimationEntry schedule[] = {
        virtualFrame<0>(2), virtualFrame<1>(3, Animation::WipeIn),
        virtualFrame<0>(65530, Animation::WalkOut)
    };
    for (uint8_t col = 0; col < COLS; ++col) {
        check(getColumn(2, col, schedule, 3) == 0, "entry local progress starts zero");
        check(getColumn(4, col, schedule, 3) == renderColumn(&frames[1], col), "entry endpoint");
        check(getColumn(65534, col, schedule, 3) == 0, "long out endpoint");
        check(getColumn(65535, col, schedule, 3) == 0, "invalid timeline index");
    }
    check(animatedColumn(nullptr, 0, Animation::None, 0, 1) == 0, "null frame");
    check(animatedColumn(&text, 0, Animation::None, 0, 0) == 0, "zero duration");
    check(animatedColumn(&text, 0, Animation::None, 3, 3) == 0, "invalid step");
    constexpr uint16_t wipeOnly = animation_detail::mask(Animation::WipeIn);
    for (uint16_t step = 0; step < 9; ++step)
        for (uint8_t col = 0; col < COLS; ++col)
            check(animatedColumn<wipeOnly>(&text, col, Animation::WipeIn, step, 9) ==
                  animatedColumn(&text, col, Animation::WipeIn, step, 9),
                  "filtered engine preserves enabled effect");

    check(animatedColumn(&text, 0, static_cast<Animation>(255), 0, 1) == 0,
          "invalid effect is safe");
    check(animatedColumn<wipeOnly>(&text, 0, Animation::WalkIn, 8, 9) == 0,
          "disabled family is rejected");
    check(animatedColumn<wipeOnly>(&text, 0, Animation::OneByOne, 0, 9) == 0,
          "one-by-one family can be excluded");
    for (uint16_t step = 0; step < 65; ++step)
        for (uint8_t col = 0; col < COLS; ++col)
            check(animatedColumn(&text, col, Animation::Blink, step, 65) ==
                (animation_detail::blinkBlank(animation_detail::stage(step, 65), step)
                    ? 0 : renderColumn(&text, col)), "blink drops whole rotations");
    std::cout << "PASS: animation endpoints, masks, shifts, mapping and timeline integration\n";
}
