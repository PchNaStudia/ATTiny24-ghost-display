#pragma once
#include "frames.hpp"

// Effect byte: family in bits 0..2; Out and opposite-border flags in bits 3..4.
namespace animation_flags {
constexpr uint8_t FAMILY = 7, WIPE = 1, WALK = 2, SNAP = 3, BLINK = 4;
constexpr uint8_t ONE_BY_ONE = 5;
constexpr uint8_t OUT = 8, REVERSE = 16;
}
enum class Animation : uint8_t {
    None = 0,
    WipeIn = 17, WipeOut = 9, WipeInReverse = 1, WipeOutReverse = 25,
    WalkIn = 2, WalkOut = 26, WalkInReverse = 18, WalkOutReverse = 10,
    Typewriter = 19, Backspace = 27, ForwardDelete = 11, Blink = 4, OneByOne = 5
};

constexpr uint8_t animationFamily(Animation effect) {
    return uint8_t(effect) & animation_flags::FAMILY;
}
constexpr bool validAnimation(Animation effect) {
    const uint8_t value = uint8_t(effect);
    const uint8_t family = animationFamily(effect);
    return value == 0 || value == animation_flags::BLINK ||
        value == animation_flags::ONE_BY_ONE ||
        ((value & ~uint8_t(31)) == 0 && family >= animation_flags::WIPE &&
         family <= animation_flags::SNAP);
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