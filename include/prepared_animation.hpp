#pragma once
#include "virtual_frames.hpp"

// Rotation-local rendering state; no expanded frames or framebuffer.
struct PreparedAnimation {
    const Frame* frame;
    uint8_t width, phase;
    uint8_t begin[2], end[2];
    int8_t shift;

    template<uint16_t EnabledEffects = ANIMATION_EFFECT_MASK>
    void prepare(const Frame* source, Animation effect, uint16_t step,
                 uint16_t duration, Strip strip) {
        frame = source;
        phase = DisplayLayout::stripPhase(strip);
        const uint8_t flags = uint8_t(effect);
        const uint8_t family = animationFamily(effect);
        width = Hal::readByte(&frame->displayMode) == uint8_t(DisplayMode::TwoHalves) ? HALF : COLS;
        const uint8_t stage = animation_detail::stage(step, duration);
        shift = 0;
        const bool reverse = flags & animation_flags::REVERSE;
        const bool walking = family == animation_flags::WALK;
        const uint8_t distance = animation_detail::distance(stage, width,
                                                           flags & animation_flags::OUT);
        if constexpr (EnabledEffects & animation_detail::WALK_MASK)
            if (walking) shift = uint8_t(reverse ? -int16_t(distance) : distance);
        for (uint8_t half = 0; half < 2; ++half) {
            const uint8_t offset = width == HALF ? half * HALF : 0;
            uint8_t begin = 0, end = width;
            if (family != 0) {
                if constexpr (EnabledEffects & animation_detail::ONE_BY_ONE_MASK) {
                    if (family == animation_flags::ONE_BY_ONE)
                        animation_detail::characterWindow(frame, width, stage, begin, end, offset);
                }
                if constexpr (EnabledEffects & animation_detail::BLINK_MASK) {
                    if (family == animation_flags::BLINK) {
                        if (animation_detail::blinkBlank(stage, step)) end = 0;
                    }
                }
                if (family < animation_flags::BLINK) {
                    if (!walking) {
                        uint8_t cutoff = reverse ? width - distance : distance;
                        if constexpr (EnabledEffects & animation_detail::SNAP_MASK) {
                            if (family == animation_flags::SNAP)
                                cutoff = animation_detail::roundToEmpty(frame, cutoff, width, offset);
                        }
                        if (reverse) end = cutoff;
                        else begin = cutoff;
                    }
                }
            }
            this->begin[half] = begin;
            this->end[half] = end;
        }
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
    uint8_t column(uint8_t column) const {
        const uint8_t position = (column + phase) & (COLS - 1);
        const bool inverted = width == HALF && position >= HALF;
        const uint8_t half = inverted ? 1 : 0;
        const uint8_t logical = inverted ? COLS - 1 - position : position;
        if (logical < begin[half] || logical >= end[half]) return 0;
        // Modulo-256 subtraction also handles the +/-128 full-width endpoint.
        const uint8_t source = uint8_t(logical - shift);
        if (source >= width) return 0;
        const uint8_t value = Hal::readByte(&frame->columns[(inverted ? HALF : 0) + source]);
        return inverted ? reverseBits(value) : value;
    }
};
