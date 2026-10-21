#pragma once
#include "frames.hpp"

// Composable operations; aliases do not introduce separate effect engines.
namespace animation_flags {
constexpr uint8_t REVEAL = 1, DELETE = 2, SNAP = 4, SHIFT = 8;
constexpr uint8_t OUT = 16, REVERSE = 32, BLINK = 64;
}
enum class Animation : uint8_t {
    None = 0,
    WipeIn = 1, WipeOut = 2, WipeInReverse = 33, WipeOutReverse = 34,
    Typewriter = 5, ForwardDelete = 6, Backspace = 38,
    WalkIn = 8, WalkInReverse = 40, WalkOut = 56, WalkOutReverse = 24,
    Blink = 64, OneByOne = 7
};
constexpr Animation operator|(Animation left, Animation right) {
    return static_cast<Animation>(uint8_t(left) | uint8_t(right));
}
constexpr bool validAnimation(Animation effect) {
    const uint8_t bits = uint8_t(effect);
    return !(bits & 128) &&
        (!(bits & animation_flags::SNAP) || (bits & 3)) &&
        (!(bits & animation_flags::OUT) || (bits & animation_flags::SHIFT)) &&
        (!(bits & animation_flags::REVERSE) || (bits & (3 | animation_flags::SHIFT)));
}

struct AnimationEntry {
    uint16_t repetitions;
    uint8_t frameIndex;
    Animation effect = Animation::None;
    Strip strip = Strip::First;
};

template<uint8_t FrameIndex>
constexpr AnimationEntry virtualFrame(uint16_t repetitions = 1,
                                     Animation effect = Animation::None,
                                     Strip strip = Strip::First) {
    // Bounds are validated against the configured frame array in animation.hpp.
    return {repetitions, FrameIndex, effect, strip};
}