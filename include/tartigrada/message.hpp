#pragma once

#include "node.hpp"
#include "platform.hpp"
#include "state.hpp"
#include "type_id.hpp"

namespace tartigrada
{

struct actor_base_t;

// Sentinel address meaning "deliver to all actors".
constexpr actor_base_t* broadcast = nullptr;

struct message_base_t : public node<message_base_t>
{
  [[nodiscard]] virtual tartigrada::size_t get_id()      const noexcept = 0;
  [[nodiscard]] constexpr actor_base_t*    get_address() const noexcept { return address_; }
  constexpr void set_address(actor_base_t* addr)               noexcept { address_ = addr; }

private:
  actor_base_t* address_ = broadcast;
};

template<typename Derived>
struct message_t : public message_base_t
{
  static constexpr tartigrada::size_t type_id = tartigrada::type_id<Derived>();
  [[nodiscard]] tartigrada::size_t get_id() const noexcept override { return type_id; }
};

struct state_message_t : public message_t<state_message_t>
{
  [[nodiscard]] constexpr State get_state() const noexcept { return state_; }
  constexpr void                set_state(State s) noexcept { state_ = s; }

private:
  State state_ = State::UNINITIALIZED;
};

} // namespace tartigrada
