#include <catch2/catch_test_macros.hpp>

#include <tartigrada/tartigrada.hpp>

using namespace tartigrada;

namespace {

// --- messages ---------------------------------------------------------------

// access_t: deferred until its bound mutex is free.
// release_t: signals the worker to release the mutex.
struct access_t  : mutex_message_t<access_t>  {};
struct release_t : message_t<release_t>        {};

// --- actor ------------------------------------------------------------------

// Models a worker that needs exclusive access to a shared counter.
//
// On init: posts its own access_msg (bound to the shared mutex).
// on_access: mutex is now free — lock it, increment shared counter, schedule release.
// on_release: release the mutex so another worker can enter.
struct worker_t : actor_base_t
{
    mutex_t&  mutex;
    int&      shared_counter;
    access_t  access_msg;
    release_t release_msg;
    int       work_count = 0;

    worker_t(environment_t& env, mutex_t& mtx, int& counter)
        : actor_base_t{ env },
          mutex{ mtx },
          shared_counter{ counter },
          accessHandler { this, &worker_t::on_access  },
          releaseHandler{ this, &worker_t::on_release }
    {
        access_msg.bind(mutex);
        subscribe(&accessHandler);
        subscribe(&releaseHandler);
    }

    void init() noexcept override
    {
        access_msg.set_address(this);
        send(&access_msg);
    }

    void on_access(access_t*) noexcept
    {
        mutex.lock();            // acquire exclusive access
        ++shared_counter;
        ++work_count;
        release_msg.set_address(this);
        send(&release_msg);      // schedule release for next cycle
    }

    void on_release(release_t*) noexcept { mutex.unlock(); }

    handler_t<decltype(&worker_t::on_access)>  accessHandler;
    handler_t<decltype(&worker_t::on_release)> releaseHandler;
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

TEST_CASE("mutex: is_ready reflects lock state", "[mutex]")
{
    mutex_t   mtx;
    access_t  msg;
    msg.bind(mtx);

    REQUIRE(msg.is_ready());

    mtx.lock();
    REQUIRE_FALSE(msg.is_ready());

    mtx.unlock();
    REQUIRE(msg.is_ready());
}

TEST_CASE("mutex: two workers get exclusive access in order", "[mutex]")
{
    fixture f;
    mutex_t mtx;
    int     counter = 0;

    worker_t a{ f.env, mtx, counter };
    worker_t b{ f.env, mtx, counter };
    f.super.add(&a);
    f.super.add(&b);

    f.run();

    REQUIRE(a.work_count   == 1);
    REQUIRE(b.work_count   == 1);
    REQUIRE(counter        == 2);
}

TEST_CASE("mutex: second worker blocked until first releases", "[mutex]")
{
    fixture f;
    mutex_t mtx;
    int     counter = 0;

    worker_t a{ f.env, mtx, counter };
    worker_t b{ f.env, mtx, counter };
    f.super.add(&a);
    f.super.add(&b);

    f.step(); // boot: supervisor → posts INIT to a and b
    f.step(); // a.init() → posts access_a (mutex free → ready)
    f.step(); // b.init() → posts access_b (mutex still free → ready)

    // access_a dispatched first: a locks mutex, increments counter, posts release_a
    f.step();
    REQUIRE(a.work_count == 1);
    REQUIRE(b.work_count == 0); // access_b blocked — mutex is locked

    // access_b not ready; release_a dispatched → mutex released
    f.step();
    REQUIRE(b.work_count == 0); // release fired but access_b not yet dispatched

    // access_b now ready → b locks mutex, increments counter, posts release_b
    f.step();
    REQUIRE(b.work_count == 1);
    REQUIRE(counter      == 2);
}

TEST_CASE("mutex: three workers serialize access", "[mutex]")
{
    fixture f;
    mutex_t mtx;
    int     counter = 0;

    worker_t a{ f.env, mtx, counter };
    worker_t b{ f.env, mtx, counter };
    worker_t c{ f.env, mtx, counter };
    f.super.add(&a);
    f.super.add(&b);
    f.super.add(&c);

    f.run();

    REQUIRE(a.work_count == 1);
    REQUIRE(b.work_count == 1);
    REQUIRE(c.work_count == 1);
    REQUIRE(counter      == 3);
}

} // namespace
