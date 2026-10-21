#include "virtual_frames.hpp"
#include <cstdlib>
#include <iostream>

static void check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(EXIT_FAILURE); }
}
static_assert(uint8_t(Animation::OneByOne) == uint8_t(Animation::Typewriter | Animation::ForwardDelete), "composition alias");
int main() {
    Frame pattern{};
    for (uint8_t col = 0; col < COLS; ++col) pattern.columns[col] = 0x81;
    for (Animation effect : {Animation::WipeIn, Animation::Typewriter, Animation::WalkIn, Animation::WalkInReverse})
        for (uint8_t col = 0; col < COLS; ++col) {
            check(animatedColumn(&pattern,col,effect,0,129)==0,"in starts blank");
            check(animatedColumn(&pattern,col,effect,128,129)==0x81,"in ends visible");
        }
    for (Animation effect : {Animation::WipeOut,Animation::ForwardDelete,Animation::Backspace,Animation::WalkOut,Animation::WalkOutReverse})
        for (uint8_t col = 0; col < COLS; ++col) {
            check(animatedColumn(&pattern,col,effect,0,129)==0x81,"out starts visible");
            check(animatedColumn(&pattern,col,effect,128,129)==0,"out ends blank");
        }
    for (Strip strip : {Strip::First,Strip::Second}) {
        Frame frame{};
        frame.displayMode = DisplayMode::TwoHalves;
        // Independent variable-width glyph runs, leading/repeated/trailing gaps.
        for (uint8_t col = 0; col < HALF; ++col) {
            if ((col >= 3 && col < 5) || (col >= 10 && col < 23) || (col >= 30 && col < 31))
                frame.columns[col] = 0x81;
            if ((col >= 1 && col < 10) || (col >= 14 && col < 16) || (col >= 60))
                frame.columns[HALF + col] = 0x24;
        }
        for (uint16_t step = 0; step < 65; ++step) {
            const uint8_t progress = animation_detail::stage(step,65);
            const uint8_t cursor = progress ? uint16_t(progress+1)>>2 : 0;

            for (uint8_t col=0;col<COLS;++col) {
                const uint8_t pos=(col+DisplayLayout::stripPhase(strip)) & (COLS-1);
                const bool lower=pos>=HALF;
                const uint8_t local=lower?COLS-1-pos:pos;
                uint8_t boundary=cursor;
                if(boundary) while(boundary<HALF && frame.columns[(lower?HALF:0)+boundary]) ++boundary;
                // Independent forward scan finds the last revealed complete run.
                uint8_t first = 0, last = 0;
                bool inside = false;
                for (uint8_t x = 0; x < boundary; ++x) {
                    const bool on = frame.columns[(lower?HALF:0)+x] != 0;
                    if (on && !inside) first = x;
                    if (on) last = x + 1;
                    inside = on;
                }
                uint8_t expected=local>=first && local<last?frame.columns[(lower?HALF:0)+local]:0;
                if(lower) expected=reverseBits(expected);
                check(animatedColumn(&frame,col,Animation::OneByOne,step,65,strip)==expected,"single complete character and physical layout");
                const bool blank=animation_detail::blinkBlank(progress,step);
                check(animatedColumn(&frame,col,Animation::OneByOne|Animation::Blink,step,65,strip)==(blank?0:expected),"blink applied after composed window");
            }
        }
    }
    // Empty input, repeated gaps, invalid inputs and durations remain safe.
    const Frame empty{};
    for(uint8_t col=0;col<COLS;++col) {
        check(animatedColumn(&empty,col,Animation::OneByOne,0,1)==0,"empty frame");
        check(animatedColumn(nullptr,col,Animation::None,0,1)==0,"null frame");
        check(animatedColumn(&pattern,col,Animation::None,1,1)==0,"invalid step");
    }
    unsigned off=0;
    for(unsigned stage=0;stage<256;++stage) off+=animation_detail::blinkBlank(stage,0);
    check(off==64,"regular blink duty");
    std::cout << "PASS: composed operations, endpoints, character breakpoints, blink and layout\n";
}
