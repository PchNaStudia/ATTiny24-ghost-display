#pragma once
#include "animation_config.hpp"
constexpr bool framesFit() {
    for (const auto& frame : frames)
        if (!frame.fits) return false;
    return true;
}
static_assert(framesFit(), "Frame text exceeds its display mode capacity");

static_assert(sizeof(frames) / sizeof(frames[0]) > 0 &&
              sizeof(frames) / sizeof(frames[0]) <= UINT8_MAX,
              "Configure 1 to 255 physical frames");
constexpr uint8_t FRAME_COUNT = sizeof(frames) / sizeof(frames[0]);

constexpr bool animationEntriesValid() {
    for (const auto& entry : animation)
        if (entry.repetitions == 0 || entry.frameIndex >= FRAME_COUNT ||
            !validAnimation(entry.effect) || uint8_t(entry.strip) > 1) return false;
    return true;
}
static_assert(animationEntriesValid(),
              "Animation requires positive repetitions and valid frame indices");

constexpr uint16_t configuredAnimationEffects() {
    uint16_t mask = 0;
    for (const auto& entry : animation)
        mask |= uint16_t(1) << animationFamily(entry.effect);
    return mask;
}
constexpr uint16_t ANIMATION_EFFECT_MASK = configuredAnimationEffects();
