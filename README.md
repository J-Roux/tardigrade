# tartigrada

A lightweight, header-only actor model framework for C++17.  
Targets bare-metal embedded systems (AVR, ARM Cortex-M) as well as hosted environments.  
Zero heap allocation, zero exceptions, zero RTTI.

## Features

- **Header-only** — drop `include/tartigrada/` into your project
- **No dynamic allocation** — all actors, messages, and handlers are statically allocated
- **Typed messages** — each message type carries its own compile-time ID (FNV-1a hash); no `dynamic_cast`
- **Semaphore-gated messages** — defer dispatch until a resource count is satisfied
- **Supervisor / cascade shutdown** — one actor retiring triggers an ordered shutdown of all children
- **ISR-safe** — queue accesses guarded by a user-supplied RAII critical section type
- **Portable** — compiles on AVR (`avr-g++`), Cortex-M, and x86/x64 host with the same source

## Concepts

| Concept | Type | Description |
|---|---|---|
| Message | `message_t<Derived>` | Typed, statically allocated message |
| Semaphore message | `semaphore_message_t<Derived>` | Dispatched only when a semaphore count > 0 |
| Handler | `handler_t<&Actor::method>` | Binds a member function to a message type |
| Actor | `actor_base_t` | Lifecycle: `init()` → operational → `shutdown()` |
| Supervisor | `supervisor_t` | Manages a list of child actors; CASCADE policy retires all on first child exit |
| Environment | `environment_t` | Message queue + handler registry; owns one event loop |
| Critical section | user-supplied RAII type | Passed as template arg to `run<CS>()`/`step<CS>()` |

## Quick start

```cpp
#include <tartigrada/tartigrada.hpp>
using namespace tartigrada;

struct ping_t : message_t<ping_t> {};
struct pong_t : message_t<pong_t> {};

struct pinger_t : actor_base_t {
    pong_t pong;
    pinger_t(environment_t& env)
        : actor_base_t{env}, h{this, &pinger_t::on_ping} { subscribe(&h); }

    void init() noexcept override { send(&pong); }
    void on_ping(ping_t*) noexcept { send(&pong); }
    handler_t<decltype(&pinger_t::on_ping)> h;
};

struct ponger_t : actor_base_t {
    ping_t ping;
    ponger_t(environment_t& env)
        : actor_base_t{env}, h{this, &ponger_t::on_pong} { subscribe(&h); }

    void on_pong(pong_t*) noexcept { send(&ping); }
    handler_t<decltype(&ponger_t::on_pong)> h;
};

int main() {
    environment_t env;
    state_message_t boot{};
    supervisor_t    super{env};
    pinger_t        ping{env};
    ponger_t        pong{env};

    super.add(&ping);
    super.add(&pong);

    boot.set_state(State::INITIALIZING);
    boot.set_address(&super);
    env.post(&boot);

    super.run();   // on Arduino: call super.step() from loop()
}
```

## ISR-safe dispatch (AVR example)

Provide a RAII critical section so the ISR can safely post into the queue
while the event loop is running:

```cpp
struct avr_cs_t {
    uint8_t sreg_;
    avr_cs_t()  noexcept : sreg_{SREG} { cli(); }
    ~avr_cs_t() noexcept { SREG = sreg_; }
};

// In main:
supervisor.run<avr_cs_t>();

// In an ISR:
env.post(&some_message);   // safe — dispatch() holds avr_cs_t around queue ops
```

## Semaphore-gated messages

Use `semaphore_message_t` to hold a message in the queue until all
prerequisites have been met:

```cpp
struct report_t : semaphore_message_t<report_t> {};

semaphore_t sem{0};
report_t    msg;
msg.bind(sem);          // msg dispatches only when sem.count() > 0

// From two independent actors:
sem.release();          // first actor done
sem.release();          // second actor done → msg now dispatches
```

## Shutdown and CASCADE policy

When a child actor calls `retire()`, the supervisor (if using
`ShutdownPolicy::CASCADE`) sends a `SHUT_DOWNING` message to every other
child, then calls its own `shutdown()`.  Ordering is deterministic:
children are shut down in reverse-add order.

```cpp
supervisor_t super{env, ShutdownPolicy::CASCADE};
super.add(&child_a);   // shuts down second
super.add(&child_b);   // shuts down first (push_front reversal)
```

## Building

### Host (tests + simulator)

Requires [Conan 2](https://conan.io/) and CMake ≥ 3.16.

```bash
conan install . --output-folder=build/Release --build=missing -s build_type=Release
cmake -B build/Release -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake
cmake --build build/Release
ctest --test-dir build/Release
```

### AVR firmware

Requires `avr-g++` (AVR-GCC).

```bash
cmake -B build-avr -DCMAKE_TOOLCHAIN_FILE=cmake/avr-toolchain.cmake
cmake --build build-avr --target arduino_watchdog
```

### Simulate AVR firmware (simavr)

Build the host tree first (provides `sim_runner`), then:

```bash
cmake --build build/Release --target sim_arduino_watchdog
```

This runs `arduino_watchdog.elf` through `sim_runner` with 30 s of simulated
AVR time.  Sleep cycles are fast-forwarded so it completes in milliseconds.
UART output is printed to stdout with simulated timestamps:

```
[    37 ms] A0-A3: 0,0,0,0  D2-D7: 0b111111
[  8229 ms] A0-A3: 0,0,0,0  D2-D7: 0b111111
[ 16421 ms] A0-A3: 0,0,0,0  D2-D7: 0b111111
[ 24613 ms] A0-A3: 0,0,0,0  D2-D7: 0b111111
```

## Examples

| File | Description |
|---|---|
| `examples/ping_pong_bare.cpp` | Minimal ping-pong between two actors (no threads, no OS) |
| `examples/ping_pong.cpp` | Ping-pong with a dedicated thread per actor (hosted) |
| `examples/arduino_watchdog.cpp` | ATmega328P periodic sensor read with WDT power-down sleep |

## License

MIT
