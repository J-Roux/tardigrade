#include <tartigrada/tartigrada.hpp>

using namespace tartigrada;

struct ping_t : message_t<ping_t> {};
struct pong_t : message_t<pong_t> {};

struct pinger_t : actor_base_t
{
  pong_t pong;


  pinger_t(enviroment_t& env)
      : actor_base_t{ env },
        pingHandler{ this, &pinger_t::ping_handler }
  {
    subscribe(&pingHandler);
  }

  void init() noexcept override { send(&pong); }

  void ping_handler(ping_t*) noexcept { send(&pong); }
  handler_t<decltype(&pinger_t::ping_handler)> pingHandler;
};

struct ponger_t : actor_base_t
{
  ping_t ping;


  ponger_t(enviroment_t& env)
      : actor_base_t{ env },
        pongHandler{ this, &ponger_t::pong_handler }
  {
    subscribe(&pongHandler);
  }

  void pong_handler(pong_t*) noexcept { send(&ping); }
  handler_t<decltype(&ponger_t::pong_handler)> pongHandler;
};


int main()
{
  enviroment_t    enviroment;
  state_message_t boot_msg;
  superviser_t    s{ enviroment };
  pinger_t        ping{ enviroment };
  ponger_t        pong{ enviroment };

  enviroment.set_top(&s);
  boot_msg.set_state(State::INITIALIZING);
  boot_msg.set_address(&s);
  enviroment.post(&boot_msg);

  s.registrate(&ping);
  s.registrate(&pong);

  s.do_process();   // on Arduino: call s.do_step() from loop() instead
}
