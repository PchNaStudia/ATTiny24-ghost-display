#include "font.hpp"
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <utility>

namespace {
int checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
void token(const char* text, int glyph, size_t consumed) {
    const auto actual = ledfont::decode(text);
    check(actual.glyph == glyph, "decoded glyph");
    check(actual.consumed == consumed, "consumed bytes");
}
using Pixels = std::set<std::pair<int, int>>;
void rendering(const std::string& text, int glyph, ledfont::RenderMode mode) {
    Pixels actual, expected;
    int calls = 0;
    const int x = -7, y = 13;
    const int width = glyph >= 0 ? ledfont::widths[glyph] : 3;
    const bool fixed = mode == ledfont::RenderMode::Fixed;
    const int shift = fixed && width < 6 ? (6 - width) / 2 : 0;
    if (glyph >= 0) {
        for (int col = 0; col < width; ++col) {
            if (fixed && col + shift >= 6) break;
            for (int row = 0; row < ledfont::fontHeight; ++row) {
                if (ledfont::bitmap[ledfont::offsets[glyph] + col] & (1u << row))
                    expected.emplace(x + shift + col, y + row);
            }
        }
    }
    const int advance = ledfont::drawText(text.c_str(), x, y,
        [&](int px, int py, bool on) {
            check(on, "callback receives only ON pixels");
            ++calls;
            actual.emplace(px, py);
        }, mode);
    check(advance == (fixed ? 6 : width + 1), "glyph advance");
    check(actual == expected, "exact glyph pixels and coordinates");
    check(calls == static_cast<int>(expected.size()), "no duplicate pixel callbacks");
}
}

int main() {
    using namespace ledfont;
    const size_t total = symbolCount + specialCount;
    check(fontHeight == 8, "font height");
    check(sizeof(availableSymbols[0]) == 1, "symbol byte size");
    check(std::string(availableSymbols).size() == symbolCount, "symbol count");
    check(sizeof(widths) / sizeof(widths[0]) == total, "width count");
    check(sizeof(offsets) / sizeof(offsets[0]) == total, "offset count");
    size_t end = 0;
    std::set<char> symbols;
    for (size_t i = 0; i < total; ++i) {
        check(widths[i] > 0, "nonempty glyph width");
        check(offsets[i] == end, "contiguous glyph offsets");
        end += widths[i];
        check(end <= sizeof(bitmap), "glyph fits bitmap");
        std::string text;
        if (i < symbolCount) {
            check(symbols.insert(availableSymbols[i]).second, "unique ordinary symbol");
            check(ordinaryIndex(availableSymbols[i]) == static_cast<int>(i), "ordinary lookup");
            text.assign(1, availableSymbols[i]);
        } else {
            text = "\\{" + std::to_string(i - symbolCount) + "}\\";
        }
        token(text.c_str(), static_cast<int>(i), text.size());
        rendering(text, static_cast<int>(i), RenderMode::Proportional);
        rendering(text, static_cast<int>(i), RenderMode::Fixed);
    }
    check(end == sizeof(bitmap), "no unused bitmap bytes");
    for (int c = 1; c < 128; ++c) {
        if (!symbols.count(static_cast<char>(c)))
            check(ordinaryIndex(static_cast<char>(c)) == -1, "unsupported ordinary lookup");
    }
    token(nullptr, -1, 0);
    token("", -1, 0);
    token("?", -1, 1);
    token("\\", -1, 1);
    token("\\\\", backslashGlyph, 2);
    token("\\{0}\\", symbolCount, 5);
    token("\\{0000}\\", symbolCount, 8);
    token("\\{1}\\", -1, 5);
    token("\\{65535}\\", -1, 9);
    token("\\{65536}\\", -1, 9);
    token("\\{999999999999999999999999}\\", -1, 28);
    for (const char* malformed : {"\\{", "\\{}\\", "\\{0", "\\{0}", "\\{-1}\\", "\\{x}\\", "\\x"})
        token(malformed, -1, 1);
    rendering("?", -1, RenderMode::Proportional);
    rendering("?", -1, RenderMode::Fixed);
    for (auto mode : {RenderMode::Proportional, RenderMode::Fixed}) {
        int calls = 0;
        auto pixel = [&](int, int, bool) { ++calls; };
        check(drawText(nullptr, 10, 20, pixel, mode) == 0, "null text advance");
        check(drawText("", 10, 20, pixel, mode) == 0, "empty text advance");
        check(calls == 0, "empty text has no pixels");
        Pixels actual, expected;
        const std::string text = "AI?\\{0}\\";
        int advance = 0;
        for (const std::string& part : {std::string("A"), std::string("I"), std::string("?"), std::string("\\{0}\\")}) {
            advance += drawText(part.c_str(), 10 + advance, 20,
                [&](int x, int y, bool) { expected.emplace(x, y); }, mode);
        }
        check(drawText(text.c_str(), 10, 20,
            [&](int x, int y, bool) { actual.emplace(x, y); }, mode) == advance,
            "mixed text advance");
        check(actual == expected, "mixed text placement");
    }
    std::cout << "PASS: " << checks << " checks, " << total << " glyphs\n";
}