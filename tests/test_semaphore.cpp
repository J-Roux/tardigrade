#include <catch2/catch_test_macros.hpp>

#include <tartigrada/tartigrada.hpp>

using namespace tartigrada;

namespace {

// --- messages ---------------------------------------------------------------

// permit_t: deferred until the semaphore has a free slot.
// done_t:   signals the worker to release its semaphore slot.
struct permit_t : semaphore_message_t<permit_t> {};
struct done_t   : message_t<done_t>             {};

// --- actor ------------------------------------------------------------------

// Worker that acquires one semaphore slot on init, does work, then releases.
struct worker_t : actor_base_t
{
    semaphore_t& sem;
    permit_t     permit_msg;
    done_t       done_msg;
    int          work_count = 0;

    worker_t(environment_t& env, semaphore_t& s)
        : actor_base_t{ env },
          sem{ s },
          handlers{ pack<&worker_t::on_permit, &worker_t::on_done>() }
    {
        permit_msg.bind(sem);
        subscribe(handlers);
    }

    void init() noexcept override
    {
        permit_msg.set_address(this);
        send(&permit_msg);
    }

    void on_permit(permit_t*) noexcept
    {
        sem.acquire();           // consume one slot (count > 0 guaranteed by is_ready)
        ++work_count;
        done_msg.set_address(this);
        send(&done_msg);         // schedule release
    }

    void on_done(done_t*) noexcept { sem.release(); }

    handler_pack_t<2> handlers;
};

// --- fixture ----------------------------------------------------------------

struct fixture
{
    environment_t   env;
    supervisor_t    super{ env };
    state_message_t boot{};

    fixture()
    {
        boot.set_state(State::INITIALIZING);
        boot.set_address(&super);
        env.post(&boot);
    }

    bool step() { return super.step(); }
    void run()  { super.run(); }
};

// --- tests ------------------------------------------------------------------

TEST_CASE("semaphore: is_ready reflects count", "[semaphore]")
{
    semaphore_t sem{ 2 };
    permit_t    msg;
    msg.bind(sem);

    REQUIRE(msg.is_ready());     // count = 2

    sem.acquire();               // count = 1
    REQUIRE(msg.is_ready());

    sem.acquire();               // count = 0
    REQUIRE_FALSE(msg.is_ready());

    sem.release();               // count = 1
    REQUIRE(msg.is_ready());
}

TEST_CASE("semaphore: try_acquire returns false when depleted", "[semaphore]")
{
    semaphore_t sem{ 1 };
    REQUIRE(sem.try_acquire());
    REQUIRE_FALSE(sem.try_acquire());
    sem.release();
    REQUIRE(sem.try_acquire());
}

TEST_CASE("semaphore(1): two workers serialize like a mutex", "[semaphore]")
{
    fixture f;
    semaphore_t sem{ 1 };

    worker_t a{ f.env, sem };
    worker_t b{ f.env, sem };
    f.super.add(&a);
    f.super.add(&b);

    f.run();

    REQUIRE(a.work_count == 1);
    REQUIRE(b.work_count == 1);
    REQUIRE(sem.count()  == 1); // fully released
}

TEST_CASE("semaphore(2): two workers run concurrently", "[semaphore]")
{
    fixture f;
    semaphore_t sem{ 2 };  // two slots — both workers can proceed without blocking

    worker_t a{ f.env, sem };
    worker_t b{ f.env, sem };
    f.super.add(&a);
    f.super.add(&b);

    f.step(); // boot
    f.step(); // a.init() → posts permit_a (slot available)
    f.step(); // b.init() → posts permit_b (slot still available)

    // both permit messages are ready (count == 2 at this point)
    f.step(); // permit_a → a acquires slot (count → 1), posts done_a
    REQUIRE(a.work_count == 1);

    f.step(); // permit_b → b acquires slot (count → 0), posts done_b
    REQUIRE(b.work_count == 1);

    f.run();  // drain done_a, done_b → count restored to 2
    REQUIRE(sem.count() == 2);
}

TEST_CASE("semaphore(2): third worker blocked until a slot is released", "[semaphore]")
{
    fixture f;
    semaphore_t sem{ 2 };

    worker_t a{ f.env, sem };
    worker_t b{ f.env, sem };
    worker_t c{ f.env, sem };
    f.super.add(&a);
    f.super.add(&b);
    f.super.add(&c);

    f.run();

    REQUIRE(a.work_count == 1);
    REQUIRE(b.work_count == 1);
    REQUIRE(c.work_count == 1);
    REQUIRE(sem.count()  == 2);
}

} // namespace
