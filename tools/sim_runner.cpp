// sim_runner.cpp — run an AVR ELF in simavr with fast-forward sleep.
//
// Usage: sim_runner <firmware.elf> [sim_usec] [--mcu=<name>] [--freq=<hz>]
//
// Simulated time defaults to 30 000 000 µs (30 s), which covers ~3 WDT cycles
// of the arduino_watchdog example.  Sleep cycles are fast-forwarded so the
// simulation completes in wall-clock milliseconds.
// UART0 output is printed to stdout as it is produced.
//
// --mcu and --freq override the values from the ELF's .mmcu section
// (needed when the section is GC'd by the linker).

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <setjmp.h>

extern "C" {
#include <simavr/avr_uart.h>
#include <simavr/sim_avr.h>
#include <simavr/sim_core.h>
#include <simavr/sim_cycle_timers.h>
#include <simavr/sim_elf.h>
#include <simavr/sim_interrupts.h>
#include <simavr/sim_io.h>
#include <simavr/sim_irq.h>
}

// ---------------------------------------------------------------------------
// Fast-forward run loop
// Instead of sleeping, advances the cycle counter so WDT and other timers
// fire at the right simulated time without blocking the host.
// ---------------------------------------------------------------------------

static int fast_run(avr_t* avr)
{
    if (avr->state == cpu_Stopped)
        return avr->state;

    avr_flashaddr_t new_pc = avr->pc;
    if (avr->state == cpu_Running)
        new_pc = avr_run_one(avr);

    avr_cycle_count_t sleep_cycles = avr_cycle_timer_process(avr);
    avr->pc = new_pc;

    if (avr->state == cpu_Sleeping) {
        if (!avr->sreg[S_I]) {
            fprintf(stderr, "sim_runner: sleep with interrupts off — stopping\n");
            return cpu_Done;
        }
        avr->cycle += 1 + sleep_cycles;   // fast-forward
    }

    if (avr->state == cpu_Running || avr->state == cpu_Sleeping)
        avr_service_interrupts(avr);

    return avr->state;
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

// Buffer UART output line-by-line; prefix each complete line with simulated time.
static char    g_line[256];
static int     g_line_pos = 0;
static avr_t*  g_avr      = nullptr;

static void uart_output_cb(avr_irq_t* /*irq*/, uint32_t value, void* /*param*/)
{
    const char c = static_cast<char>(value);
    if (c == '\r') return;   // skip CR in CRLF pairs

    if (c == '\n' || g_line_pos == static_cast<int>(sizeof(g_line) - 1)) {
        g_line[g_line_pos] = '\0';
        const uint64_t ms = g_avr
            ? (g_avr->cycle * 1000ULL / g_avr->frequency)
            : 0ULL;
        printf("[%6llu ms] %s\n", (unsigned long long)ms, g_line);
        fflush(stdout);
        g_line_pos = 0;
    } else {
        g_line[g_line_pos++] = c;
    }
}

static jmp_buf g_stop;

static avr_cycle_count_t stop_cb(avr_t* /*avr*/, avr_cycle_count_t /*when*/,
                                  void* /*param*/)
{
    longjmp(g_stop, 1);
    return 0;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <firmware.elf> [sim_usec] [--mcu=<name>] [--freq=<hz>]\n",
                argv[0]);
        return 1;
    }

    unsigned long sim_usec  = 30000000UL;
    const char*   cli_mcu   = nullptr;
    uint32_t      cli_freq  = 0;

    // Parse positional and named args (order-independent after argv[1]).
    for (int i = 2; i < argc; ++i) {
        if (strncmp(argv[i], "--mcu=", 6) == 0)
            cli_mcu = argv[i] + 6;
        else if (strncmp(argv[i], "--freq=", 7) == 0)
            cli_freq = static_cast<uint32_t>(strtoul(argv[i] + 7, nullptr, 10));
        else
            sim_usec = strtoul(argv[i], nullptr, 10);
    }

    // Load ELF — MCU name and frequency come from the AVR_MCU() section
    // embedded in the firmware by avr_mcu_section.h.
    elf_firmware_t fw = {};
    if (elf_read_firmware(argv[1], &fw) < 0) {
        fprintf(stderr, "sim_runner: failed to load %s\n", argv[1]);
        return 1;
    }

    // CLI args override (or supply) what the ELF section may be missing.
    if (cli_mcu)  snprintf(fw.mmcu, sizeof(fw.mmcu), "%s", cli_mcu);
    if (cli_freq) fw.frequency = cli_freq;

    fprintf(stderr, "sim_runner: mcu=%s freq=%u Hz  simulating %.1f s\n",
            fw.mmcu, fw.frequency, sim_usec / 1e6);

    avr_t* avr = avr_make_mcu_by_name(fw.mmcu);
    if (!avr) {
        fprintf(stderr, "sim_runner: unknown MCU '%s'\n", fw.mmcu);
        return 1;
    }

    avr_init(avr);
    avr_load_firmware(avr, &fw);
    avr->log = LOG_ERROR;   // suppress simavr internal noise
    g_avr = avr;

    // Disable UART stdio echo (simavr mirrors UART to its own console by default).
    {
        uint32_t flags = 0;
        avr_ioctl(avr, AVR_IOCTL_UART_GET_FLAGS('0'), &flags);
        flags &= ~AVR_UART_FLAG_STDIO;
        avr_ioctl(avr, AVR_IOCTL_UART_SET_FLAGS('0'), &flags);
    }

    // Wire UART0 output to stdout.
    avr_irq_t* uart_out =
        avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_OUTPUT);
    if (uart_out)
        avr_irq_register_notify(uart_out, uart_output_cb, nullptr);

    // Stop after sim_usec simulated microseconds.
    avr_cycle_timer_register_usec(avr, sim_usec, stop_cb, nullptr);

    if (setjmp(g_stop) == 0) {
        for (;;) {
            const int state = fast_run(avr);
            if (state == cpu_Done || state == cpu_Crashed) {
                fprintf(stderr, "sim_runner: cpu %s\n",
                        state == cpu_Done ? "done" : "crashed");
                break;
            }
        }
    } else {
        fprintf(stderr, "sim_runner: %.1f s simulated — done\n",
                sim_usec / 1e6);
    }

    avr_terminate(avr);
    return 0;
}
