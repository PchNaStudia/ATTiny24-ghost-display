#pragma once
#include "virtual_frames.hpp"

struct PreparedAnimation : AnimationState {
    template<uint16_t EnabledEffects = ANIMATION_EFFECT_MASK>
    void prepare(const Frame* source, Animation effect, uint16_t step,
                 uint16_t duration, Strip strip) {
        AnimationState::prepare<EnabledEffects>(source, effect, step, duration, strip);
    }
    Strip prepare(VirtualFrameIndex position) {
        uint16_t step = 0, duration = 0;
        const AnimationEntry* entry = nullptr;
        const Frame* source = virtual_frames_detail::resolve(position, animation,
            sizeof(animation) / sizeof(animation[0]), &step, &entry, &duration);
        const Strip strip = static_cast<Strip>(Hal::readByte(&entry->strip));
        prepare(source, static_cast<Animation>(Hal::readByte(&entry->effect)), step, duration, strip);
        return strip;
    }
};
