# tartigrada

A lightweight, header-only actor model framework for C++17 inspired by [cpp-rotor](https://github.com/basiliscos/cpp-rotor)
Designed for bare-metal embedded systems (AVR, ARM Cortex-M) and hosted environments alike.  
Zero heap allocation. Zero exceptions. Zero RTTI.

## Features

- **Header-only** — drop `include/tartigrada/` into your project, done
- **No dynamic allocation** — actors, messages, handlers, and queues are all statically allocated using intrusive linked lists
- **Typed messages** — each message type gets a compile-time ID (FNV-1a hash of `__PRETTY_FUNCTION__`); no `dynamic_cast`
- **Deferred dispatch** — override `is_ready()` on any message to hold it in the queue until a condition is met (semaphore, mutex, timer, ...)
- **Semaphore-gated messages** — `semaphore_message_t` dispatches only when a `semaphore_t` count is non-zero
- **Mutex-gated messages** — `mutex_message_t` dispatches only when a `mutex_t` is free
- **Supervisor / cascade shutdown** — one actor calling `retire()` triggers an ordered shutdown of all siblings
- **ISR-safe** — queue operations are guarded by a user-supplied RAII critical section type
- **Portable** — same source compiles on AVR (`avr-g++`), Cortex-M, and x86/x64

## Concepts

| Type | Role |
|---|---|
| `message_t<Derived>` | Typed, statically allocated message; `get_id()` returns a compile-time FNV-1a hash |
| `semaphore_message_t<Derived>` | Message that stays in the queue until `semaphore_t::count() > 0` |
| `mutex_message_t<Derived>` | Message that stays in the queue until a `mutex_t` is free |
| `handler_t` | Type-erased handler; bind a single member function via `on<&Actor::method>()` |
| `handler_pack_t<N>` | Inline array of N type-erased handlers; bind multiple via `pack<&Actor::f, &Actor::g>()` |
| `actor_base_t` | Base class for actors; overridable `init()` and `shutdown()` hooks |
| `supervisor_t` | Manages a list of child actors; drives the `CASCADE` or `REBOOT` policy |
| `environment_t` | Owns the message queue and handler registry; runs the dispatch loop |
| Critical section | User-supplied RAII type passed as `run<CS>()`; default is a no-op |

## Lifecycle

```
INITIALIZING → OPERATIONAL → SHUT_DOWNING → UNINITIALIZED
```

When a supervisor receives `INITIALIZING`, it first initialises all children (in add order), then calls its own `init()`. When a child calls `retire()`, or when the supervisor receives `SHUT_DOWNING`, children are shut down in the same add order and then the supervisor calls its own `shutdown()`.

**CASCADE policy** — any child retiring immediately triggers a cascade: all other children are sent `SHUT_DOWNING`, then the supervisor shuts itself down.

**REBOOT policy** — a child that retires is re-initialised individually; siblings are unaffected.

## Quick start

```cpp
#include <tartigrada/tartigrada.hpp>
using namespace tartigrada;

struct ping_t : message_t<ping_t> {};
struct pong_t : message_t<pong_t> {};

struct pinger_t : actor_base_t {
    pong_t    pong;
    handler_t h;

    pinger_t(environment_t& env)
        : actor_base_t{env}, h{on<&pinger_t::on_ping>()} { subscribe(&h); }

    void init() noexcept override  { send(&pong); }
    void on_ping(ping_t*) noexcept { send(&pong); }
};

struct ponger_t : actor_base_t {
    ping_t    ping;
    handler_t h;

    ponger_t(environment_t& env)
        : actor_base_t{env}, h{on<&ponger_t::on_pong>()} { subscribe(&h); }

    void on_pong(pong_t*) noexcept { send(&ping); }
};

environment_t   env;
state_message_t boot{};
supervisor_t    super{env};
pinger_t        ping{env};
ponger_t        pong{env};

int main() {
    super.add(&ping);
    super.add(&pong);

    boot.set_state(State::INITIALIZING);
    boot.set_address(&super);
    env.post(&boot);

    super.run();          // blocks until queue drains
    // on Arduino: call super.step() from loop() instead
}
```

## ISR-safe dispatch

Supply a RAII critical section so ISRs can safely call `post()` while the event loop runs:

```cpp
struct avr_cs_t {
    uint8_t sreg_;
    avr_cs_t()  noexcept : sreg_{SREG} { cli(); }
    ~avr_cs_t() noexcept { SREG = sreg_; }
};

// main loop:
supervisor.run<avr_cs_t>();

// ISR:
ISR(WDT_vect) {
    env.post(&some_message);   // safe — dispatch() holds avr_cs_t around queue access
}
```

`dispatch()` acquires the critical section only around `front()`/`pop_front()`/`push_back()` calls, not around the handler call itself. On AVR, handler bodies therefore run with interrupts **disabled** — this is intentional and lets handlers manipulate hardware registers atomically without extra `cli()`/`sei()` pairs.

## Deferred messages

Any message can override `is_ready()` to defer its own dispatch:

```cpp
// Time-delayed message (works with any Clock satisfying std Clock concept):
struct delayed_t : message_t<delayed_t> {
    std::chrono::steady_clock::time_point fire_at;
    bool is_ready() noexcept override {
        return std::chrono::steady_clock::now() >= fire_at;
    }
};
```

`dispatch()` scans the queue up to its current length each call; messages that return `false` from `is_ready()` are moved to the back and retried next call.

### Semaphore-gated messages

```cpp
struct report_t : semaphore_message_t<report_t> {};

semaphore_t sem{0};
report_t    msg;
msg.bind(sem);

// from two independent actors:
sem.release();   // first  actor signals
sem.release();   // second actor signals → msg.is_ready() now true, dispatch fires
```

### Mutex-gated messages

```cpp
struct write_t : mutex_message_t<write_t> {};

mutex_t  bus_lock;
write_t  msg;
msg.bind(bus_lock);

bus_lock.lock();       // acquire before sending
send(&msg);            // queued; dispatches once bus_lock.unlock() is called
```

## Broadcast

Setting an address of `nullptr` delivers a message to every handler that accepts its type:

```cpp
msg.set_address(tartigrada::broadcast);  // nullptr
env.post(&msg);
```

## Building

### Host (tests + sim runner)

Requires [Conan 2](https://conan.io/) and CMake ≥ 3.16.

```bash
conan install . --build=missing -s build_type=Release
cmake --preset conan-release
cmake --build build/Release
ctest --test-dir build/Release
```

### Host with simavr support

The `with_simavr=True` option builds and installs simavr from source automatically if not already present.

```bash
conan build . --build=missing -s build_type=Release -o "&:with_simavr=True"
```

### AVR firmware

Requires `avr-g++` (AVR-GCC toolchain).

```bash
cmake -B build-avr -DCMAKE_TOOLCHAIN_FILE=cmake/avr-toolchain.cmake
cmake --build build-avr --target arduino_watchdog
```

### Simulate AVR firmware

Build the host tree, then the AVR firmware, then run the simulation:

```bash
# 1. Build host (sim_runner + simavr)
conan build . --build=missing -s build_type=Release -o "&:with_simavr=True"

# 2. Build AVR firmware
cmake -B build-avr -DCMAKE_TOOLCHAIN_FILE=cmake/avr-toolchain.cmake \
    -DTARTIGRADA_WITH_SIMAVR=ON -DSIMAVR_INSTALL_PREFIX=$PWD/build/simavr
cmake --build build-avr --target arduino_watchdog

# 3. Simulate
cmake --build build/Release --target sim_arduino_watchdog
```

`sim_runner` fast-forwards sleep cycles so 30 s of simulated AVR time completes in milliseconds. UART output is printed to stdout with simulated timestamps:

```
[    37 ms] A0-A3: 0,0,0,0  D2-D7: 0b111111
[  8229 ms] A0-A3: 0,0,0,0  D2-D7: 0b111111
[ 16421 ms] A0-A3: 0,0,0,0  D2-D7: 0b111111
[ 24613 ms] A0-A3: 0,0,0,0  D2-D7: 0b111111
```

## Examples

| File | Description |
|---|---|
| `examples/ping_pong_bare.cpp` | Minimal ping-pong, compiles on both host and AVR |
| `examples/ping_pong.cpp` | Ping-pong with a dedicated `std::thread` per actor (hosted) |
| `examples/arduino_watchdog.cpp` | ATmega328P periodic sensor read with WDT power-down sleep and simavr support |

## License

MIT

