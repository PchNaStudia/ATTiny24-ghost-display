#pragma once
#include "animation_types.hpp"

// Edit the display content here. Frame indices follow the order in this array.
constexpr Frame frames[] HAL_STORAGE = {
    makeFrame("HELLO WORLD", FrameAlignment::Top),
    makeTwoHalves(makeHalfFrame("HELLO"), makeHalfFrame("WORLDS")),
    makeFrame("GOODBYE", FrameAlignment::Right),
    makeTwoHalves(makeHalfFrame("GOOD"), makeHalfFrame("BYE")),
};

// virtualFrame<frame index>(number of rotations, optional Animation::effect)
constexpr AnimationEntry animation[] HAL_STORAGE = {
    virtualFrame<0>(128, Animation::WalkInReverse),
    virtualFrame<0>(255, Animation::None, Strip::Second),
    virtualFrame<0>(128, Animation::WipeOutReverse),
    virtualFrame<1>(32, Animation::Typewriter),
    virtualFrame<1>(64, Animation::Typewriter, Strip::Second),
    virtualFrame<1>(32, Animation::Typewriter),
    virtualFrame<1>(255, Animation::OneByOne, Strip::Second),
    virtualFrame<1>(128, Animation::ForwardDelete),

    virtualFrame<2>(128, Animation::WalkInReverse),
    virtualFrame<2>(255, Animation::None, Strip::Second),
    virtualFrame<2>(128, Animation::WipeOutReverse),
    virtualFrame<3>(32, Animation::Typewriter),
    virtualFrame<3>(64, Animation::Typewriter, Strip::Second),
    virtualFrame<3>(32, Animation::Typewriter),
    virtualFrame<3>(255, Animation::OneByOne, Strip::Second),
    virtualFrame<3>(128, Animation::ForwardDelete),
};
