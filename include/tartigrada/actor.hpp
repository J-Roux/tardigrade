#pragma once

#include "environment.hpp"
#include "handler.hpp"
#include "message.hpp"
#include "node.hpp"
#include "state.hpp"

namespace tartigrada
{

struct supervisor_t; // for friend

struct actor_base_t : node<actor_base_t>
{
  friend struct supervisor_t;

  explicit actor_base_t(environment_t& environment)
      : message_{},
        environment_{ environment },
        state_{ State::INITIALIZING },
        stateHandler_{on<&actor_base_t::on_state>() }
  {
    subscribe(&stateHandler_);
  }

  [[nodiscard]] constexpr State get_state() const noexcept { return state_; }

protected:
  environment_t& environment_;

  virtual void init()     noexcept {}
  virtual void shutdown() noexcept {}

  virtual void on_state(state_message_t* s) noexcept
  {
    switch (s->get_state())
    {
    case State::INITIALIZING:
      init();
      state_ = State::OPERATIONAL;
      break;
    case State::SHUT_DOWNING:
      shutdown();
      state_ = State::UNINITIALIZED;
      message_.set_state(state_);
      if (supervisor_ && !supervised_shutdown_)
      {
        message_.set_address(supervisor_);
        send(&message_);
      }
      supervised_shutdown_ = false;
      break;
    default:
      break;
    }
  }

  void send(message_base_t* msg) noexcept  { environment_.post(msg); }
  void subscribe(handler_ptr* h) noexcept  { environment_.subscribe(h); }

  template<tartigrada::size_t N>
  void subscribe(handler_pack_t<N>& pack) noexcept
  {
    for (auto& h : pack.items) subscribe(&h);
  }

  template<auto MemFn>
  handler_t on() noexcept
  {
    return handler_t{ this, handler_t::trampoline<MemFn>() };
  }

  template<auto... MemFns>
  handler_pack_t<sizeof...(MemFns)> pack() noexcept
  {
    handler_pack_t<sizeof...(MemFns)> p;
    tartigrada::size_t i = 0;
    ((p.items[i++] = handler_t{ this, handler_t::trampoline<MemFns>() }), ...);
    return p;
  }

  void retire() noexcept
  {
    message_.set_state(State::SHUT_DOWNING);
    message_.set_address(this);
    send(&message_);
  }

private:
  state_message_t message_;
  State           state_;
  actor_base_t*   supervisor_          = nullptr;
  bool            supervised_shutdown_ = false;

  handler_t stateHandler_;
};

} // namespace tartigrada
