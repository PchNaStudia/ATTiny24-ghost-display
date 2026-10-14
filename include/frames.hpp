#pragma once
#include <stdint.h>
#include "Hal.hpp"
#include "DisplayLayout.hpp"

#include "font.hpp"

constexpr uint8_t COLS = DisplayLayout::columns;
//static_assert((COLS & (COLS - 1)) == 0, "COLS must be a power of two");
constexpr uint8_t HALF = COLS / 2;

enum class DisplayMode : uint8_t {
    FullCircle,
    TwoHalves
};

struct Frame {
    uint8_t columns[COLS];
    DisplayMode displayMode = DisplayMode::FullCircle;
    bool fits = true;
};

template<uint8_t Width>
struct TextFrame {
    uint8_t columns[Width]{};
    bool fits = true;
};
using HalfFrame = TextFrame<HALF>;

template<uint8_t Width>
constexpr TextFrame<Width> makeTextFrame(const char* text)
{
    TextFrame<Width> frame{};
    const int advance = ledfont::drawText(
        text, 0, 0,
        [&](int x, int y, bool on) constexpr {
            if (on && x >= 0 && x < Width &&
                y >= 0 && y < 8) {
                frame.columns[x] |= DisplayLayout::fontRowMask(uint8_t(y));
            }
        }
    );
    // The renderer includes one trailing spacer column.
    frame.fits = advance == 0 || advance - 1 <= Width;

    return frame;
}
enum class TextAlignment : uint8_t { Left, Center, Right };
constexpr HalfFrame makeHalfFrame(const char* text, TextAlignment alignment = TextAlignment::Center) {
    auto frame = makeTextFrame<HALF>(text);
    const int advance = ledfont::drawText(text, 0, 0, [](int, int, bool) constexpr {});
    const int used = advance > 0 ? advance - 1 : 0;
    const int spare = used < HALF ? HALF - used : 0;
    const int shift = alignment == TextAlignment::Center ? spare / 2 :
                      alignment == TextAlignment::Right ? spare : 0;
    if (shift) for (int col = HALF - 1; col >= 0; --col)
        frame.columns[col] = col >= shift ? frame.columns[col - shift] : 0;
    return frame;
}

constexpr Frame makeFrame(const char* text) {
    const auto content = makeTextFrame<COLS>(text);
    Frame frame{};
    frame.fits = content.fits;
    for (uint8_t col = 0; col < COLS; ++col) frame.columns[col] = content.columns[col];
    return frame;
}

// Independent upper/lower content; no duplicated half or expanded storage.
constexpr Frame makeTwoHalves(const HalfFrame& upper, const HalfFrame& lower) {
    Frame frame{};
    frame.displayMode = DisplayMode::TwoHalves;
    frame.fits = upper.fits && lower.fits;
    for (uint8_t col = 0; col < HALF; ++col) {
        frame.columns[col] = upper.columns[col];
        frame.columns[HALF + col] = lower.columns[col];
    }
    return frame;
}


constexpr uint8_t reverseBits(uint8_t b) {
    b = (b >> 4) | (b << 4);
    b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2);
    b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1);
    return b;
}

template<class ReadColumn>
inline uint8_t renderColumnWith(const Frame* frame, uint8_t currentCol,
                              ReadColumn readColumn, Strip strip = Strip::First) {
    if (!frame) return 0;
    const DisplayMode mode = static_cast<DisplayMode>(
        Hal::readByte(&frame->displayMode)
    );

    const uint8_t phase = DisplayLayout::stripPhase(strip);

    const uint8_t pos =
        (currentCol + phase) & (COLS - 1);

    uint8_t index = pos;
    bool inverted = false;

    if (mode == DisplayMode::TwoHalves) {
        // Sensor zero is the left edge of the upper half. The lower half
        // reverses both text direction and radial order to remain upright.
        const uint8_t p =
            (pos) & (COLS - 1);

        inverted = p >= HALF;

        index = inverted
            ? HALF + COLS - 1 - p
            : p;
    }

    uint8_t value = readColumn(index);

    return inverted ? reverseBits(value) : value;
}

inline uint8_t renderColumn(const Frame* frame, uint8_t currentCol, Strip strip = Strip::First) {
    return renderColumnWith(frame, currentCol, [&](uint8_t index) {
        return Hal::readByte(&frame->columns[index]);
    }, strip);
}

