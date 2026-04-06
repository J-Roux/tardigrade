#include <tartigrada/tartigrada.hpp>

#include <cstdio>
#include <thread>
#include <chrono>

using namespace tartigrada;

struct ping_t : public message_t<ping_t> {};
struct pong_t : public message_t<pong_t> {};

struct pinger_t : public actor_base_t
{
  pong_t pong;

  pinger_t(environment_t& environment)
      : actor_base_t{ environment },
        pong{},
        pingHandler{ this, &pinger_t::ping_handler }
  {
    subscribe(&pingHandler);
  }

  void init() noexcept override
  {
    std::printf("pinger init\n");
    send(&pong);
  }

  void ping_handler(ping_t*) noexcept
  {
    send(&pong);
    std::this_thread::sleep_for(std::chrono::seconds{ 1 });
    std::printf("pong\n");
  }

  handler_t<decltype(&pinger_t::ping_handler)> pingHandler;
};

struct ponger_t : public actor_base_t
{
  ping_t ping;

  ponger_t(environment_t& environment)
      : actor_base_t{ environment },
        ping{},
        pongHandler{ this, &ponger_t::pong_handler }
  {
    subscribe(&pongHandler);
  }

  void init() noexcept override
  {
    std::printf("ponger init\n");
  }

  void pong_handler(pong_t*) noexcept
  {
    send(&ping);
    std::this_thread::sleep_for(std::chrono::seconds{ 1 });
    std::printf("ping\n");
  }

  handler_t<decltype(&ponger_t::pong_handler)> pongHandler;
};

int main()
{
  environment_t environment;

  supervisor_t s{ environment };

  state_message_t message{};
  message.set_state(State::INITIALIZING);
  message.set_address(&s);
  environment.post(&message);

  pinger_t ping{ environment };
  ponger_t pong{ environment };
  s.add(&ping);
  s.add(&pong);

  s.run();
}
