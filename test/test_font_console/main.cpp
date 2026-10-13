#include "font.hpp"
#include <iostream>
#include <string>
#include <vector>

int main() {
    for (unsigned i = 0; i < ledfont::symbolCount + ledfont::specialCount; ++i) {
        const bool ordinary = i < ledfont::symbolCount;
        const unsigned special = ordinary ? 0 : i - ledfont::symbolCount;
        const std::string text = ordinary
            ? std::string(1, ledfont::availableSymbols[i])
            : "\\{" + std::to_string(special) + "}\\";
        std::vector<std::string> rows(ledfont::fontHeight, std::string(ledfont::widths[i], '.'));
        ledfont::drawText(text.c_str(), 0, 0,
            [&](int x, int y, bool on) { if (on) rows.at(y).at(x) = '#'; });
        std::cout << "Glyph " << i << " | ";
        if (ordinary) std::cout << "'" << text << "'";
        else std::cout << "special " << special << (special == 0 ? " (heart)" : "") << " [" << text << "]";
        std::cout << " | " << unsigned(ledfont::widths[i]) << "x" << unsigned(ledfont::fontHeight) << '\n';
        for (const std::string& row : rows) {
            std::cout << row << '\n';
        }
        std::cout << '\n';
    }
    std::cout << "PASS: printed " << ledfont::symbolCount + ledfont::specialCount << " glyphs\n";
}