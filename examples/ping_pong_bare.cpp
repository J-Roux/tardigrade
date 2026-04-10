#include <tartigrada/tartigrada.hpp>

using namespace tartigrada;

struct ping_t : public message_t<ping_t> {};
struct pong_t : public message_t<pong_t> {};

struct pinger_t : actor_base_t
{
  pong_t pong;


  pinger_t(environment_t& env)
      : actor_base_t{ env },
        pingHandler{on<&pinger_t::ping_handler>() }
  {
    subscribe(&pingHandler);
  }

  void init() noexcept override { send(&pong); }

  void ping_handler(ping_t*) noexcept { send(&pong); }
  handler_t pingHandler;
};

struct ponger_t : actor_base_t
{
  ping_t ping;


  ponger_t(environment_t& env)
      : actor_base_t{ env },
        pongHandler{on<&ponger_t::pong_handler>() }
  {
    subscribe(&pongHandler);
  }

  void pong_handler(pong_t*) noexcept { send(&ping); }
  handler_t pongHandler;
};


int main()
{
  environment_t    environment;
  state_message_t boot_msg;
  supervisor_t    s{ environment };
  pinger_t        ping{ environment };
  ponger_t        pong{ environment };

  boot_msg.set_state(State::INITIALIZING);
  boot_msg.set_address(&s);
  environment.post(&boot_msg);

  s.add(&ping);
  s.add(&pong);

  s.run();   // on Arduino: call s.step() from loop() instead
}
