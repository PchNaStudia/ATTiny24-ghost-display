#include "Hal.hpp"
#include "DisplayController.hpp"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>

namespace {
constexpr uint8_t STRIP_MASK = (1 << PB0) | (1 << PB1);
uint16_t timerOverflows = 0;
}

namespace Hal {
uint8_t readByte(const void* address) { return pgm_read_byte(address); }
uint16_t readWord(const void* address) { return pgm_read_word(address); }
uint32_t timestamp() {
    const uint8_t saved = SREG;
    cli();
    uint16_t high = timerOverflows;
    const uint16_t low = TCNT1;
    if ((TIFR1 & (1 << TOV1)) && low < 0x8000) ++high;
    SREG = saved;
    return (uint32_t(high) << 16) | low;
}
void writeColumn(uint8_t bits) { PORTA = bits; }
void cancelColumn() { TIMSK1 &= ~(1 << OCIE1A); }
bool scheduleColumn(uint16_t deadline) {
    OCR1A = deadline;
    TIFR1 = (1 << OCF1A);
    const uint16_t remaining = deadline - TCNT1;
    if (remaining < 8 || remaining > 32767u) {
        cancelColumn();
        return false;
    }
    TIMSK1 |= (1 << OCIE1A);
    return true;
}
void selectStrip(Strip strip) {
    PORTB = (PORTB & ~STRIP_MASK) |
        (strip == Strip::First ? (1 << PB0) : (1 << PB1));
}
void initialize() {
    cli();
    timerOverflows = 0;
    PORTA = 0;
    DDRA = 0xFF;
    PORTB &= ~STRIP_MASK;
    DDRB |= STRIP_MASK;
    DDRB &= ~(1 << PB2);
    PORTB &= ~(1 << PB2);
    MCUCR = (MCUCR & ~(1 << ISC00)) | (1 << ISC01);
    GIFR = (1 << INTF0);
    GIMSK |= (1 << INT0);
    TCCR1A = 0;
    TCCR1B = (1 << CS11) | (1 << CS10);
    TCNT1 = 0;
    cancelColumn();
    TIFR1 = (1 << TOV1) | (1 << OCF1A);
    TIMSK1 |= (1 << TOIE1);
    set_sleep_mode(SLEEP_MODE_IDLE);
}
void enableEvents() { sei(); }
void idle() { sleep_mode(); }
}

ISR(TIM1_COMPA_vect) { DisplayController::onColumnDeadline(); }
ISR(TIM1_OVF_vect) {
    ++timerOverflows;
    DisplayController::onTimerOverflow();
}
ISR(INT0_vect) { DisplayController::onRotation(); }