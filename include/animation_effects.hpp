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
constexpr uint16_t mask(Animation effect) { return uint8_t(effect); }
}

// Fixed order: reveal -> delete -> shift -> blink -> physical layout.
struct AnimationState {
    const Frame* frame;
    uint8_t width, phase;
    uint8_t begin[2], end[2];
    uint8_t shift;

    template<uint16_t EnabledEffects = 0x7F>
    void prepare(const Frame* source, Animation effect, uint16_t step,
                 uint16_t duration, Strip strip) {
        frame = source;
        phase = DisplayLayout::stripPhase(strip) - Hal::readByte(&frame->origin);
        width = Hal::readByte(&frame->displayMode) == uint8_t(DisplayMode::TwoHalves) ? HALF : COLS;
        const uint8_t flags = uint8_t(effect) & EnabledEffects;
        // Exact progress with column-quantized boundaries.
        const uint8_t progress = animation_detail::stage(step, duration);
        const uint8_t cursor = progress == 0 ? 0 : uint16_t(progress + 1) >> (width == HALF ? 2 : 1);
        shift = 0;
        if constexpr (EnabledEffects & animation_flags::SHIFT) {
            if (flags & animation_flags::SHIFT) {
                const uint8_t amount = flags & animation_flags::OUT ? cursor : width - cursor;
                shift = flags & animation_flags::REVERSE ? uint8_t(-amount) : amount;
            }
        }
        for (uint8_t half = 0; half < 2; ++half) {
            const uint8_t offset = half * (width & HALF);
            uint8_t first = 0, last = width;
            if constexpr (EnabledEffects & (animation_flags::REVEAL | animation_flags::DELETE)) {
                uint8_t boundary = cursor;
                if constexpr (EnabledEffects & animation_flags::SNAP)
                    if (flags & animation_flags::SNAP)
                        boundary = animation_detail::roundToEmpty(frame, boundary, width, offset);
                if (flags & animation_flags::REVEAL) last = boundary;
                if (flags & animation_flags::DELETE) {
                    first = boundary;
                    if (flags & animation_flags::REVEAL) {
                        // Reuse the snapped reveal breakpoint; keep exactly its
                        // preceding nonempty run, holding it across spacer gaps.
                        while (first && Hal::readByte(&frame->columns[offset + first - 1]) == 0) --first;
                        last = first;
                        while (first && Hal::readByte(&frame->columns[offset + first - 1]) != 0) --first;
                    }

                }
                if (flags & animation_flags::REVERSE) {
                    const uint8_t oldFirst = first;
                    first = width - last;
                    last = width - oldFirst;
                }
            }
            if constexpr (EnabledEffects & animation_flags::BLINK)
                if ((flags & animation_flags::BLINK) && animation_detail::blinkBlank(progress, step)) last = 0;
            begin[half] = first;
            end[half] = last;
        }
    }
    uint8_t column(uint8_t col) const {
        const uint8_t position = (col + phase) & (COLS - 1);
        // position < 128: bit 6 selects a half only when width == 64.
        const uint8_t half = (position & width) >> 6;
        const bool inverted = half;
        const uint8_t logical = inverted ? COLS - 1 - position : position;
        const uint8_t source = logical - shift;
        // end never exceeds width, so this also rejects shifted out-of-range columns.
        if (source < begin[half] || source >= end[half]) return 0;
        const uint8_t value = Hal::readByte(&frame->columns[(inverted ? HALF : 0) + source]);
        return inverted ? reverseBits(value) : value;
    }
};

template<uint16_t EnabledEffects = 0x7F>
inline uint8_t animatedColumn(const Frame* frame, uint8_t column,
                             Animation effect, uint16_t step, uint16_t duration,
                             Strip strip = Strip::First) {
    if (!frame || !duration || step >= duration || !validAnimation(effect) ||
        (uint8_t(effect) & ~EnabledEffects)) return 0;
    AnimationState state{};
    state.prepare<EnabledEffects>(frame, effect, step, duration, strip);
    return state.column(column);
}
