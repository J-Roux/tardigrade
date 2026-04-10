// Periodic sensing with power-down sleep — ATmega328P bare-metal example.
//
// Cycle (repeats every WDT interval, ~8 s):
//
//   boot / WDT fires
//     → watchdog_t.init()       — configure WDT interrupt mode
//     → power_manager_t.init()  — enable USART0 + ADC
//     → analog_reader_t.init()  — read A0-A3, notify reporter
//     → digital_reader_t.init() — read D2-D7, notify reporter
//     → reporter_t.on_report()  — UART report, uart_flush(), retire()
//     → CASCADE shutdown (children in add-reverse order):
//         power_manager_t.shutdown() — disable USART0 + ADC
//         digital_reader_t.shutdown()
//         analog_reader_t.shutdown()
//         reporter_t.shutdown()      — semaphore reset
//     → watchdog_t.do_sleep()   — SLEEP_MODE_PWR_DOWN until next WDT
//
// watchdog_t IS the supervisor.  sleep is a regular message posted at the
// end of shutdown(), so it dispatches after all children have cleaned up.
//
// Target : ATmega328P @ 16 MHz
// Build  : avr-g++ -mmcu=atmega328p -DF_CPU=16000000UL -Os

#include <tartigrada/tartigrada.hpp>

#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "avr_hal.hpp"

// Embed MCU name and clock frequency in the ELF so simavr can auto-detect them.
#if __has_include(<avr/avr_mcu_section.h>)
#  include <avr/avr_mcu_section.h>
   AVR_MCU(F_CPU, "atmega328p");
#endif


using namespace tartigrada;

// ---------------------------------------------------------------------------
// AVR critical section
// ---------------------------------------------------------------------------

struct avr_cs_t
{
    uint8_t sreg_;
    avr_cs_t()  noexcept : sreg_{SREG} { cli(); }
    ~avr_cs_t() noexcept { SREG = sreg_; }
};

// WDT ISR defined after actor structs (needs complete watchdog_t type).

// ---------------------------------------------------------------------------
// Actors
// ---------------------------------------------------------------------------

// Manages USART0 and ADC power.
struct power_manager_t : actor_base_t
{
    explicit power_manager_t(environment_t& env) : actor_base_t{ env } {}

    void init() noexcept override
    {
        power_usart0_enable();
        uart_init();
        power_adc_enable();
        adc_init();
    }

    void shutdown() noexcept override
    {
        // UART was already flushed by reporter before retire(); just cut power.
        UCSR0B = 0;
        power_usart0_disable();
        power_adc_disable();
    }
};

// Collects sensor readings and sends a UART report, then retires.
// report_msg is gated on report_sem: dispatches only after both readers have
// released the semaphore (count reaches 2).
struct reporter_t : actor_base_t
{
    uint16_t    analog_ch[4] = {};
    uint8_t     digital_mask  = 0;
    semaphore_t report_sem{ 0 };

    struct report_t : semaphore_message_t<report_t> {};
    report_t report_msg;

    explicit reporter_t(environment_t& env)
        : actor_base_t{ env },
          reportHandler{on<&reporter_t::on_report>() }
    {
        report_msg.bind(report_sem);
        report_msg.set_address(this);
        subscribe(&reportHandler);
    }

    void init() noexcept override     { send(&report_msg); }
    void shutdown() noexcept override { report_sem.reset(); }

private:
    void on_report(report_t*) noexcept
    {
        uart_puts("A0-A3: ");
        for (uint8_t i = 0; i < 4; ++i)
        {
            uart_put_u16(analog_ch[i]);
            if (i < 3) uart_putc(',');
        }
        uart_puts("  D2-D7: 0b");
        for (int8_t bit = 5; bit >= 0; --bit)
            uart_putc(static_cast<char>('0' + ((digital_mask >> bit) & 1)));
        uart_puts("\r\n");

        uart_flush();   // complete transmission before CASCADE cuts power
        retire();
    }

    handler_t reportHandler;
};

// Reads A0-A3; stores in reporter and releases the report semaphore.
struct analog_reader_t : actor_base_t
{
    reporter_t& reporter_;

    analog_reader_t(environment_t& env, reporter_t& rep)
        : actor_base_t{ env }, reporter_{ rep } {}

    void init() noexcept override
    {
        for (uint8_t i = 0; i < 4; ++i)
            reporter_.analog_ch[i] = adc_read(i);
        reporter_.report_sem.release();
    }
};

// Reads D2-D7; stores in reporter and releases the report semaphore.
struct digital_reader_t : actor_base_t
{
    reporter_t& reporter_;

    digital_reader_t(environment_t& env, reporter_t& rep)
        : actor_base_t{ env }, reporter_{ rep } {}

    void init() noexcept override
    {
        reporter_.digital_mask = gpio_read_d2_d7();
        reporter_.report_sem.release();
    }
};

// Supervisor that owns the full wake/sense/sleep cycle.
// init()     — configures WDT in interrupt mode (~8 s).
// shutdown() — posts sleep_msg AFTER children's shutdown messages so
//              do_sleep() dispatches last, once all peripherals are off.
struct watchdog_t : supervisor_t
{
    struct sleep_t : message_t<sleep_t> {};

    explicit watchdog_t(environment_t& env)
        : supervisor_t{ env, ShutdownPolicy::CASCADE },
          sleepHandler{on<&watchdog_t::do_sleep>() }
    {
        sleep_msg.set_address(this);
        subscribe(&sleepHandler);
    }

    void init() noexcept override
    {
        avr_cs_t cs;
        wdt_init_interrupt_8s();
    }

    void shutdown() noexcept override
    {
        send(&sleep_msg);   // queued after children's shutdown msgs → dispatches last
    }

private:
    void do_sleep(sleep_t*) noexcept
    {
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        sleep_enable();
        sei();
        sleep_cpu();        // blocks until WDT fires
        sleep_disable();
    }

    sleep_t  sleep_msg;
    handler_t sleepHandler;
};

// ---------------------------------------------------------------------------
// Global objects
// ---------------------------------------------------------------------------

environment_t    env;


reporter_t       reporter  { env };
analog_reader_t  analog_rd { env, reporter };
digital_reader_t digital_rd{ env, reporter };
power_manager_t  power_mgr { env };
watchdog_t       watchdog  { env };   // extends supervisor_t; constructed last
state_message_t  boot{&watchdog, State::INITIALIZING};
// ---------------------------------------------------------------------------
// WDT ISR — re-arm interrupt mode, then post boot to start the actor cycle
// ---------------------------------------------------------------------------

ISR(WDT_vect)
{
    wdt_rearm_interrupt();
    env.post(&boot);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main()
{
    gpio_init();

    // push_back reverses add order → init sequence: power_mgr, analog_rd,
    // digital_rd, reporter.  power_mgr must initialise peripherals first.
    watchdog.add(&power_mgr);
    watchdog.add(&digital_rd);
    watchdog.add(&analog_rd);
    watchdog.add(&reporter);
    env.post(&boot);

    while(true)
    {
        watchdog.run<avr_cs_t>();
    }
}
