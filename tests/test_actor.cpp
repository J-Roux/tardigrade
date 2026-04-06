#include <catch2/catch_test_macros.hpp>

#include <tartigrada/tartigrada.hpp>

using namespace tartigrada;

namespace {

// --- helpers ----------------------------------------------------------------

struct ping_t : message_t<ping_t> {};
struct pong_t : message_t<pong_t> {};

struct recorder_t : actor_base_t
{
    int init_count     = 0;
    int shutdown_count = 0;
    int ping_count     = 0;

   

    explicit recorder_t(environment_t& env)
        : actor_base_t{ env },
          pingHandler{ this, &recorder_t::on_ping }
    {
        subscribe(&pingHandler);
    }

    void init()     noexcept override { ++init_count; }
    void shutdown() noexcept override { ++shutdown_count; }

    void on_ping(ping_t*) noexcept { ++ping_count; }
    
    handler_t<decltype(&recorder_t::on_ping)> pingHandler;
};

// Builds a minimal environment with a supervisor and delivers the
// INITIALIZING boot message, then runs until the queue drains.
struct fixture
{
    environment_t     env;
    supervisor_t     super{ env };
    state_message_t  boot{};

    fixture()
    {
        boot.set_state(State::INITIALIZING);
        boot.set_address(&super);
        env.post(&boot);
    }

    void run() { super.run(); }

    void shutdown()
    {
        boot.set_state(State::SHUT_DOWNING);
        boot.set_address(&super);
        env.post(&boot);
        run();
    }
};

// Actor that retires exactly once on init, then stays up on any subsequent init.
struct retiring_t : actor_base_t
{
    int init_count     = 0;
    int shutdown_count = 0;
    bool retired       = false;

    explicit retiring_t(environment_t& env) : actor_base_t{ env } {}

    void init() noexcept override
    {
        ++init_count;
        if (!retired) { retired = true; retire(); }
    }
    void shutdown() noexcept override { ++shutdown_count; }
};

// fixture variant with a configurable supervisor policy
struct policy_fixture
{
    environment_t    env;
    supervisor_t    super;
    state_message_t boot{};

    explicit policy_fixture(ShutdownPolicy p)
        : super{ env, p }
    {
        boot.set_state(State::INITIALIZING);
        boot.set_address(&super);
        env.post(&boot);
    }

    void run() { super.run(); }
};

// --- tests ------------------------------------------------------------------

TEST_CASE("actor init() is called on INITIALIZING", "[actor]")
{
    fixture f;
    recorder_t rec{ f.env };
    f.super.add(&rec);

    f.run();

    REQUIRE(rec.init_count == 1);
}

TEST_CASE("actor shutdown() is called on SHUT_DOWNING", "[actor]")
{
    fixture f;
    recorder_t rec{ f.env };
    f.super.add(&rec);

    f.run();
    f.shutdown();

    REQUIRE(rec.shutdown_count == 1);
}

TEST_CASE("actor state transitions correctly", "[actor]")
{
    fixture f;
    recorder_t rec{ f.env };
    f.super.add(&rec);

    REQUIRE(rec.get_state() == State::INITIALIZING);
    f.run();
    REQUIRE(rec.get_state() == State::OPERATIONAL);
    f.shutdown();
    REQUIRE(rec.get_state() == State::UNINITIALIZED);
}

TEST_CASE("handler receives matching message", "[actor]")
{
    fixture f;
    recorder_t rec{ f.env };
    f.super.add(&rec);
    f.run();

    ping_t ping;
    f.env.post(&ping);
    f.run();

    REQUIRE(rec.ping_count == 1);
}

TEST_CASE("handler ignores non-matching message", "[actor]")
{
    fixture f;
    recorder_t rec{ f.env };
    f.super.add(&rec);
    f.run();

    pong_t pong;  // recorder has no pong handler
    f.env.post(&pong);
    f.run();

    REQUIRE(rec.ping_count == 0);
}

TEST_CASE("addressed message only reaches its target actor", "[actor]")
{
    fixture f;
    recorder_t a{ f.env }, b{ f.env };
    f.super.add(&a);
    f.super.add(&b);
    f.run();

    ping_t ping;
    ping.set_address(&a);
    f.env.post(&ping);
    f.run();

    REQUIRE(a.ping_count == 1);
    REQUIRE(b.ping_count == 0);
}

// --- broadcast tests --------------------------------------------------------

TEST_CASE("broadcast reaches all actors", "[broadcast]")
{
    fixture f;
    recorder_t a{ f.env }, b{ f.env }, c{ f.env };
    f.super.add(&a);
    f.super.add(&b);
    f.super.add(&c);
    f.run();

    ping_t ping;
    // default address is broadcast (nullptr) — no set_address call needed
    f.env.post(&ping);
    f.run();

    REQUIRE(a.ping_count == 1);
    REQUIRE(b.ping_count == 1);
    REQUIRE(c.ping_count == 1);
}

TEST_CASE("broadcast reaches actors across supervisor hierarchy", "[broadcast]")
{
    fixture f;
    supervisor_t child{ f.env };
    recorder_t a{ f.env }, b{ f.env };

    f.super.add(&child);
    child.add(&a);
    f.super.add(&b);
    f.run();

    ping_t ping;
    f.env.post(&ping);
    f.run();

    REQUIRE(a.ping_count == 1);
    REQUIRE(b.ping_count == 1);
}

TEST_CASE("broadcast does not interfere with addressed message", "[broadcast]")
{
    fixture f;
    recorder_t a{ f.env }, b{ f.env };
    f.super.add(&a);
    f.super.add(&b);
    f.run();

    ping_t ping_all;                   // broadcast
    ping_t ping_a;
    ping_a.set_address(&a);            // addressed

    f.env.post(&ping_all);
    f.env.post(&ping_a);
    f.run();

    REQUIRE(a.ping_count == 2);        // got both
    REQUIRE(b.ping_count == 1);        // got only broadcast
}

TEST_CASE("broadcast address constant equals nullptr", "[broadcast]")
{
    STATIC_REQUIRE(broadcast == nullptr);
}

// --- supervisor tests -------------------------------------------------------

TEST_CASE("supervisor initialises all children", "[supervisor]")
{
    fixture f;
    recorder_t a{ f.env }, b{ f.env };
    f.super.add(&a);
    f.super.add(&b);

    f.run();

    REQUIRE(a.init_count == 1);
    REQUIRE(b.init_count == 1);
}

TEST_CASE("supervisor shuts down all children", "[supervisor]")
{
    fixture f;
    recorder_t a{ f.env }, b{ f.env };
    f.super.add(&a);
    f.super.add(&b);

    f.run();
    f.shutdown();

    REQUIRE(a.shutdown_count == 1);
    REQUIRE(b.shutdown_count == 1);
}

// --- hierarchical supervisor tests ------------------------------------------

TEST_CASE("child supervisor transitively initialises grandchildren", "[supervisor][hierarchy]")
{
    fixture f;
    supervisor_t child{ f.env };
    recorder_t a{ f.env }, b{ f.env };

    f.super.add(&child);
    child.add(&a);
    child.add(&b);

    f.run();

    REQUIRE(a.init_count == 1);
    REQUIRE(b.init_count == 1);
}

TEST_CASE("child supervisor reaches OPERATIONAL when parent initialises", "[supervisor][hierarchy]")
{
    fixture f;
    supervisor_t child{ f.env };
    recorder_t a{ f.env };

    f.super.add(&child);
    child.add(&a);

    f.run();

    REQUIRE(child.get_state() == State::OPERATIONAL);
    REQUIRE(a.get_state() == State::OPERATIONAL);
}

TEST_CASE("child supervisor transitively shuts down grandchildren", "[supervisor][hierarchy]")
{
    fixture f;
    supervisor_t child{ f.env };
    recorder_t a{ f.env }, b{ f.env };

    f.super.add(&child);
    child.add(&a);
    child.add(&b);

    f.run();
    f.shutdown();

    REQUIRE(a.shutdown_count == 1);
    REQUIRE(b.shutdown_count == 1);
    REQUIRE(child.get_state() == State::UNINITIALIZED);
}

TEST_CASE("broadcast message reaches grandchildren", "[supervisor][hierarchy]")
{
    fixture f;
    supervisor_t child{ f.env };
    recorder_t a{ f.env };

    f.super.add(&child);
    child.add(&a);
    f.run();

    ping_t ping;  // no address — broadcast
    f.env.post(&ping);
    f.run();

    REQUIRE(a.ping_count == 1);
}

TEST_CASE("3-level hierarchy initialises all actors", "[supervisor][hierarchy]")
{
    fixture f;                          // root
    supervisor_t mid{ f.env };
    supervisor_t leaf_sup{ f.env };
    recorder_t a{ f.env };

    f.super.add(&mid);
    mid.add(&leaf_sup);
    leaf_sup.add(&a);

    f.run();

    REQUIRE(mid.get_state() == State::OPERATIONAL);
    REQUIRE(leaf_sup.get_state() == State::OPERATIONAL);
    REQUIRE(a.init_count   == 1);
}

TEST_CASE("siblings under different supervisors initialise independently", "[supervisor][hierarchy]")
{
    fixture f;
    supervisor_t left{ f.env }, right{ f.env };
    recorder_t a{ f.env }, b{ f.env };

    f.super.add(&left);
    f.super.add(&right);
    left.add(&a);
    right.add(&b);

    f.run();

    REQUIRE(a.init_count == 1);
    REQUIRE(b.init_count == 1);
    REQUIRE(a.shutdown_count == 0);
    REQUIRE(b.shutdown_count == 0);
}

// --- shutdown policy tests --------------------------------------------------

TEST_CASE("REBOOT policy re-initialises a child that retires", "[policy]")
{
    policy_fixture f{ ShutdownPolicy::REBOOT };
    retiring_t actor{ f.env };
    f.super.add(&actor);

    f.run();

    // init once → retires → supervisor reboots it → init again
    REQUIRE(actor.init_count     == 2);
    REQUIRE(actor.shutdown_count == 1);
    REQUIRE(actor.get_state() == State::OPERATIONAL);
}

TEST_CASE("REBOOT policy leaves other children untouched", "[policy]")
{
    policy_fixture f{ ShutdownPolicy::REBOOT };
    retiring_t  quitter{ f.env };
    recorder_t  stable{ f.env };
    f.super.add(&quitter);
    f.super.add(&stable);

    f.run();

    REQUIRE(quitter.init_count     == 2);
    REQUIRE(stable.init_count      == 1);
    REQUIRE(stable.shutdown_count  == 0);
    REQUIRE(f.super.get_state() == State::OPERATIONAL);
}

TEST_CASE("CASCADE policy shuts supervisor down when a child retires", "[policy]")
{
    policy_fixture f{ ShutdownPolicy::CASCADE };
    retiring_t actor{ f.env };
    f.super.add(&actor);

    f.run();

    REQUIRE(actor.shutdown_count == 1);
    REQUIRE(f.super.get_state() == State::UNINITIALIZED);
}

TEST_CASE("CASCADE policy shuts down sibling actors", "[policy]")
{
    policy_fixture f{ ShutdownPolicy::CASCADE };
    retiring_t  quitter{ f.env };
    recorder_t  sibling{ f.env };
    f.super.add(&quitter);
    f.super.add(&sibling);

    f.run();

    REQUIRE(sibling.shutdown_count == 1);
    REQUIRE(f.super.get_state() == State::UNINITIALIZED);
}

TEST_CASE("CASCADE propagates through hierarchy", "[policy]")
{
    policy_fixture f{ ShutdownPolicy::CASCADE };
    supervisor_t child{ f.env, ShutdownPolicy::CASCADE };
    retiring_t   quitter{ f.env };
    recorder_t   sibling{ f.env };

    f.super.add(&child);
    child.add(&quitter);
    child.add(&sibling);

    f.run();

    REQUIRE(sibling.shutdown_count == 1);
    REQUIRE(child.get_state() == State::UNINITIALIZED);
    REQUIRE(f.super.get_state() == State::UNINITIALIZED);
}

} // namespace
