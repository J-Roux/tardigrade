#include <catch2/catch_test_macros.hpp>

#include <tartigrada/tartigrada.hpp>

#include <chrono>
#include <cstdio>

using namespace tartigrada;
using namespace std::chrono_literals;

namespace {

// --- fake clock -------------------------------------------------------------
// Models the std Clock concept: static now(), controllable via advance().

struct fake_clock_t
{
    using duration   = std::chrono::nanoseconds;
    using rep        = duration::rep;
    using period     = duration::period;
    using time_point = std::chrono::time_point<fake_clock_t>;
    static constexpr bool is_steady = true;

    [[nodiscard]] static time_point now() noexcept { return time_point(epoch_) ; }
    static void advance(duration d) noexcept       { epoch_ += d; }
    static void reset()             noexcept       { epoch_ = duration::zero(); }

private:
    inline static duration epoch_ = duration::zero();
};

using time_point = fake_clock_t::time_point;
using duration   = fake_clock_t::duration;

// --- messages ---------------------------------------------------------------
namespace {
struct ping_t : message_t<ping_t> {};

struct pong_t : message_t<pong_t>
{
    time_point fire_at;

    explicit pong_t(duration delay)
        : fire_at{ fake_clock_t::now() + delay } {}

    [[nodiscard]] bool is_ready() noexcept override
    {
        const auto n = fake_clock_t::now();
        const bool rdy = n >= fire_at;
        std::printf("[pong] is_ready: now=%lld fire_at=%lld -> %s\n",
            (long long)n.time_since_epoch().count(),
            (long long)fire_at.time_since_epoch().count(),
            rdy ? "YES" : "NO");
        return rdy;
    }
};
}

// --- actors -----------------------------------------------------------------

struct pinger_t : actor_base_t
{
    duration delay;
    int      ping_count = 0;
    pong_t   pong;

    pinger_t(environment_t& env, duration d)
        : actor_base_t{ env },
          delay{ d },
          pong{ d },
          pingHandler{ this, &pinger_t::on_ping }
    {
        subscribe(&pingHandler);
    }

    void init() noexcept override
    {
        std::printf("[pinger] init, posting pong with fire_at=%lld\n",
            (long long)pong.fire_at.time_since_epoch().count());
        send(&pong);
    }

    void on_ping(ping_t*) noexcept
    {
        ++ping_count;
        pong.fire_at = fake_clock_t::now() + delay;
        std::printf("[pinger] got ping #%d, resetting pong fire_at=%lld\n",
            ping_count, (long long)pong.fire_at.time_since_epoch().count());
        send(&pong);
    }

    handler_t<decltype(&pinger_t::on_ping)> pingHandler;
};

struct ponger_t : actor_base_t
{
    int  pong_count = 0;
    ping_t ping;

    explicit ponger_t(environment_t& env)
        : actor_base_t{ env },
          pongHandler{ this, &ponger_t::on_pong }
    {
        subscribe(&pongHandler);
    }

    void on_pong(pong_t*) noexcept
    {
        ++pong_count;
        std::printf("[ponger] got pong #%d, sending ping\n", pong_count);
        send(&ping);
    }

    handler_t<decltype(&ponger_t::on_pong)> pongHandler;
};

// --- fixture ----------------------------------------------------------------

struct fixture
{
    environment_t   env;
    supervisor_t    super{ env };
    state_message_t boot{};

    fixture()
    {
        fake_clock_t::reset();
        boot.set_state(State::INITIALIZING);
        boot.set_address(&super);
        env.post(&boot);
    }

    void run() { super.run(); }

    void step() { std::ignore = super.step(); }
};

// --- tests ------------------------------------------------------------------

TEST_CASE("pong is delayed until timer elapses", "[timer]")
{
    fixture f;
    pinger_t pinger{ f.env, 5ms };
    ponger_t ponger{ f.env };
    f.super.add(&pinger);
    f.super.add(&ponger);
    f.run();

    REQUIRE(ponger.pong_count == 0); // timer not elapsed yet

    fake_clock_t::advance(10ms);
    f.run(); // pong fires → ponger sends ping → pinger receives ping
    REQUIRE(ponger.pong_count == 1);
    REQUIRE(pinger.ping_count == 1);
}

TEST_CASE("pong does not fire before delay", "[timer]")
{
    fixture f;
    pinger_t pinger{ f.env, 50ms };
    ponger_t ponger{ f.env };
    f.super.add(&pinger);
    f.super.add(&ponger);
    f.run();

    fake_clock_t::advance(49ms);
    f.run();
    REQUIRE(ponger.pong_count == 0);
}

TEST_CASE("ping-pong exchanges N times with timer", "[timer]")
{
    fixture f;
    pinger_t pinger{ f.env, 10ms };
    ponger_t ponger{ f.env };
    f.super.add(&pinger);
    f.super.add(&ponger);
    f.run();

    for (int i = 0; i < 5; ++i)
    {
        fake_clock_t::advance(10ms);
        f.run();
    }

    REQUIRE(ponger.pong_count == 5);
    REQUIRE(pinger.ping_count == 5);
}

} // namespace
