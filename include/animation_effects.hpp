#pragma once
#include "animation_types.hpp"

namespace animation_detail {
// Exact floor(step * 255 / (duration - 1)), using 16-bit arithmetic only.
inline
    uint8_t stage(uint16_t step, uint16_t duration) {
    if (duration <= 1 || step >= duration - 1) return 255;
    const uint16_t denominator = duration - 1;
    uint16_t remainder = step;
    uint8_t result = 0;
    for (uint8_t bit = 0; bit < 8; ++bit) {
        result <<= 1;
        if (remainder >= denominator - remainder) {
            remainder -= denominator - remainder;
            ++result;
        } else remainder += remainder;
    }
    // Convert floor(256 * fraction) into floor(255 * fraction).
    if (remainder < step) --result;
    return result;
}

// Distance away from the unmodified frame: In runs border -> origin.
inline uint8_t distance(uint8_t stage, uint8_t width, bool out) {
    const uint8_t amount = out ? stage : uint8_t(255 - stage);
    static_assert(COLS == 128 && HALF == 64, "Distance shifts require 128/64 columns");
    if (amount == 0) return 0;
    const uint16_t scaled = uint16_t(amount) + 1;
    return uint8_t(width == HALF ? scaled >> 2 : scaled >> 1);
}
inline int shiftedColumn(uint8_t column, uint8_t distance, bool reverse) {
    return reverse ? int(column) + distance : int(column) - distance;
}
inline uint8_t roundToEmpty(const Frame* frame, uint8_t cutoff, uint8_t width,
                            uint8_t offset = 0) {
    if (cutoff == 0) return 0;
    while (cutoff < width && Hal::readByte(&frame->columns[offset + cutoff]) != 0) ++cutoff;
    return cutoff;
}
inline bool maskedColumn(uint8_t column, uint8_t cutoff, bool reverse) {
    return reverse ? column >= cutoff : column < cutoff;
}
// Four evenly spaced off intervals, each lasting 1/16 of the entry.
// Progress 0 and 255 remain visible; all columns share the same decision.
inline bool blinkBlank(uint8_t stage, uint16_t /*step*/) {
    return (stage & 63) >= 32 && (stage & 63) < 48;
}
constexpr uint16_t mask(Animation effect) { return uint16_t(1) << animationFamily(effect); }
constexpr uint16_t WALK_MASK = uint16_t(1) << animation_flags::WALK;
constexpr uint16_t SNAP_MASK = uint16_t(1) << animation_flags::SNAP;
constexpr uint16_t BLINK_MASK = uint16_t(1) << animation_flags::BLINK;
constexpr uint16_t ONE_BY_ONE_MASK = uint16_t(1) << animation_flags::ONE_BY_ONE;

// Typewriter's snapped reveal boundary, with deletion one character behind.
// Empty gaps hold the last revealed character; the initial boundary is blank.
inline void characterWindow(const Frame* frame, uint8_t width, uint8_t stage,
                            uint8_t& begin, uint8_t& end, uint8_t offset = 0) {
    end = roundToEmpty(frame, width - distance(stage, width, false), width, offset);
    begin = end;
    while (begin && Hal::readByte(&frame->columns[offset + begin - 1]) == 0) --begin;
    end = begin;
    while (begin && Hal::readByte(&frame->columns[offset + begin - 1]) != 0) --begin;
}
}

// Family filtering is compile-time; direction/progress remain runtime flags.
template<uint16_t EnabledEffects = 0xFFFF>
inline uint8_t animatedColumn(const Frame* frame, uint8_t column,
                             Animation effect, uint16_t step, uint16_t duration,
                             Strip strip = Strip::First) {
    if (!frame || duration == 0 || step >= duration) return 0;
    const uint8_t flags = uint8_t(effect);
    const uint8_t family = animationFamily(effect);
    if (flags > 31 || family > animation_flags::ONE_BY_ONE ||
        (family == 0 && flags != 0) ||
        (family >= animation_flags::BLINK && flags != family)) return 0;
    if constexpr (!(EnabledEffects & (uint16_t(1) << animation_flags::WIPE)))
        if (family == animation_flags::WIPE) return 0;
    if constexpr (!(EnabledEffects & animation_detail::WALK_MASK))
        if (family == animation_flags::WALK) return 0;
    if constexpr (!(EnabledEffects & animation_detail::SNAP_MASK))
        if (family == animation_flags::SNAP) return 0;
    if constexpr (!(EnabledEffects & animation_detail::BLINK_MASK))
        if (family == animation_flags::BLINK) return 0;
    if constexpr (!(EnabledEffects & animation_detail::ONE_BY_ONE_MASK))
        if (family == animation_flags::ONE_BY_ONE) return 0;
    const uint8_t width = Hal::readByte(&frame->displayMode) == uint8_t(DisplayMode::TwoHalves)
        ? HALF : COLS;
    const uint8_t position = (column + DisplayLayout::stripPhase(strip)) & (COLS - 1);
    const uint8_t offset = width == HALF && position >= HALF ? HALF : 0;
    uint8_t begin = 0, end = width;
    int16_t shift = 0;
    if (family != 0) {
        const uint8_t stage = animation_detail::stage(step, duration);
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
            const bool reverse = flags & animation_flags::REVERSE;
            const uint8_t distance = animation_detail::distance(stage, width,
                                                               flags & animation_flags::OUT);
            bool walking = false;
            if constexpr (EnabledEffects & animation_detail::WALK_MASK) {
                walking = family == animation_flags::WALK;
                if (walking) shift = reverse ? -int16_t(distance) : int16_t(distance);
            }
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
    const bool inverted = offset != 0;
    const uint8_t logical = inverted ? COLS - 1 - position : position;
    if (logical < begin || logical >= end) return 0;
    const int source = int(logical) - shift;
    if (source < 0 || source >= width) return 0;
    const uint8_t value = Hal::readByte(&frame->columns[offset + source]);
    return inverted ? reverseBits(value) : value;
}

