#pragma once
#include "animation.hpp"
#include "animation_effects.hpp"

using VirtualFrameIndex = uint16_t;

namespace virtual_frames_detail {
constexpr uint32_t timelineLength() {
    uint32_t total = 0;
    for (const auto& entry : animation) total += entry.repetitions;
    return total;
}
inline uint16_t repetitions(const AnimationEntry& entry) {
    return Hal::readWord(&entry.repetitions);
}
}

static_assert(virtual_frames_detail::timelineLength() > 0 &&
              virtual_frames_detail::timelineLength() <= UINT16_MAX,
              "Animation must contain 1 to 65535 virtual frames");
constexpr VirtualFrameIndex VIRTUAL_FRAME_COUNT = virtual_frames_detail::timelineLength();

inline VirtualFrameIndex nextVirtualFrame(VirtualFrameIndex current) {
    return current >= VIRTUAL_FRAME_COUNT - 1 ? 0 : current + 1;
}

namespace virtual_frames_detail {
// Schedule is stored in program memory on AVR; no expanded timeline is allocated.
inline const Frame* resolve(VirtualFrameIndex position,
                            const AnimationEntry* schedule, uint16_t count,
                            uint16_t* localStep = nullptr,
                            const AnimationEntry** selected = nullptr,
                            uint16_t* selectedDuration = nullptr) {
    for (uint16_t i = 0; i < count; ++i) {
        const uint16_t duration = repetitions(schedule[i]);
        if (position < duration) {
            if (localStep) *localStep = position;
            if (selected) *selected = &schedule[i];
            if (selectedDuration) *selectedDuration = duration;
            const uint8_t index = Hal::readByte(&schedule[i].frameIndex);
            return index < FRAME_COUNT ? &frames[index] : nullptr;
        }
        position -= duration;
    }
    return nullptr;
}
inline const Frame* resolve(VirtualFrameIndex position) {
    return resolve(position, animation, sizeof(animation) / sizeof(animation[0]));
}
}

inline Strip getStrip(VirtualFrameIndex position) {
    const AnimationEntry* entry = nullptr;
    const Frame* frame = virtual_frames_detail::resolve(position, animation,
        sizeof(animation) / sizeof(animation[0]), nullptr, &entry);
    return frame ? static_cast<Strip>(Hal::readByte(&entry->strip)) : Strip::First;
}

template<uint16_t EnabledEffects = 0xFFFF>
inline uint8_t getColumn(VirtualFrameIndex position, uint8_t column,
                        const AnimationEntry* schedule, uint16_t count) {
    uint16_t step = 0;
    uint16_t duration = 0;
    const AnimationEntry* entry = nullptr;
    const Frame* frame = virtual_frames_detail::resolve(position, schedule, count, &step, &entry, &duration);
    if (!frame) return 0;
    return animatedColumn<EnabledEffects>(frame, column,
        static_cast<Animation>(Hal::readByte(&entry->effect)), step,
        duration,
        static_cast<Strip>(Hal::readByte(&entry->strip)));
}

inline uint8_t getColumn(VirtualFrameIndex position, uint8_t column) {
    if constexpr ((ANIMATION_EFFECT_MASK & ~uint16_t(1)) == 0)
        return renderColumn(virtual_frames_detail::resolve(position), column, getStrip(position));
    else return getColumn<ANIMATION_EFFECT_MASK>(position, column, animation,
                     sizeof(animation) / sizeof(animation[0]));
}

