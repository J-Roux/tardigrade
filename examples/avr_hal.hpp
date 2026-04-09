#pragma once

// avr_hal.hpp — ATmega328P peripheral helpers (UART0, ADC, GPIO).

#include <avr/io.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// UART0 — 9600 baud @ F_CPU
// ---------------------------------------------------------------------------

namespace {

static constexpr uint16_t UBRR_9600 = static_cast<uint16_t>(F_CPU / (16UL * 9600UL) - 1);

void uart_init() noexcept
{
    UBRR0H = static_cast<uint8_t>(UBRR_9600 >> 8);
    UBRR0L = static_cast<uint8_t>(UBRR_9600);
    UCSR0B = (1 << TXEN0);                     // TX only
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);   // 8N1
}

void uart_putc(char c) noexcept
{
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = static_cast<uint8_t>(c);
}

void uart_puts(const char* s) noexcept
{
    while (*s) uart_putc(*s++);
}

void uart_put_u16(uint16_t v) noexcept
{
    char buf[6];
    uint8_t pos = 5;
    buf[pos] = '\0';
    do { buf[--pos] = static_cast<char>('0' + v % 10); v /= 10; } while (v);
    uart_puts(buf + pos);
}

void uart_flush() noexcept
{
    while (!(UCSR0A & (1 << TXC0)));
    UCSR0A |= (1 << TXC0);   // clear flag
}

// ---------------------------------------------------------------------------
// ADC — AVcc reference, prescaler /128
// ---------------------------------------------------------------------------

void adc_init() noexcept
{
    ADMUX  =  (1 << REFS0);
    ADCSRA =  (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    ADCSRA |= (1 << ADSC);                   // discard first conversion (settling)
    while (ADCSRA & (1 << ADSC));
}

uint16_t adc_read(uint8_t ch) noexcept
{
    ADMUX  = (ADMUX & 0xF0) | (ch & 0x0F);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

// ---------------------------------------------------------------------------
// WDT — interrupt mode, ~8 s timeout
// ---------------------------------------------------------------------------

// Call with interrupts disabled. Configures WDT to fire an interrupt (not
// reset) every ~8 s. Caller is responsible for re-enabling interrupts.
// Re-arm WDT interrupt mode after it fires (hardware clears WDIE automatically).
void wdt_rearm_interrupt() noexcept
{
    WDTCSR |= (1 << WDIE);
}

void wdt_init_interrupt_8s() noexcept
{
    MCUSR  &= ~(1 << WDRF);
    WDTCSR |=  (1 << WDCE) | (1 << WDE);
    WDTCSR  =  (1 << WDIE) | (1 << WDP3) | (1 << WDP0);
}

// ---------------------------------------------------------------------------
// GPIO — D2-D7 = PD2-PD7, inputs with pull-ups
// ---------------------------------------------------------------------------

void gpio_init() noexcept
{
    DDRD  &= ~0xFC;    // bits 2-7 as input
    PORTD |=  0xFC;    // enable pull-ups
}

// Returns a 6-bit mask: bit 0 = D2 (PD2), bit 5 = D7 (PD7)
uint8_t gpio_read_d2_d7() noexcept
{
    return static_cast<uint8_t>((PIND >> 2) & 0x3F);
}

} // namespace
