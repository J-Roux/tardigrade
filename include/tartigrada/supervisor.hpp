#pragma once

#include "actor.hpp"
#include "queue.hpp"

namespace tartigrada
{

enum class ShutdownPolicy { REBOOT, CASCADE };

struct supervisor_t : public actor_base_t
{
  supervisor_t(environment_t& environment, ShutdownPolicy policy = ShutdownPolicy::CASCADE)
      : actor_base_t{ environment }, policy_{ policy }
  {
    environment_.set_supervisor(this);
  }

  void add(actor_base_t* actor) noexcept
  {
    childs_.push_front(actor);
    actor->supervisor_ = this;
  }

  template<class CS = empty_critical_section_t>
  [[nodiscard]] bool step() noexcept { return environment_.dispatch<CS>(); }

  template<class CS = empty_critical_section_t>
  void run() noexcept { while (step<CS>()); }

protected:
  void on_state(state_message_t* s) noexcept override
  {
    switch (s->get_state())
    {
    case State::INITIALIZING:
      child_init();
      init();
      state_ = State::OPERATIONAL;
      break;

    case State::SHUT_DOWNING:
      child_shutdown();
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

    case State::UNINITIALIZED:
      on_child_terminated(s);
      break;

    default:
      break;
    }
  }

private:
  queue<actor_base_t> childs_;
  ShutdownPolicy      policy_;

  void change_child_state(State child_state) noexcept
  {
    for (auto* ptr : childs_)
    {
      if (child_state == State::SHUT_DOWNING)
      {
        if (ptr->state_ == State::UNINITIALIZED) continue;
        ptr->supervised_shutdown_ = true;
      }
      ptr->message_.set_address(ptr);
      ptr->message_.set_state(child_state);
      send(&ptr->message_);
    }
  }

  void child_init()     noexcept { change_child_state(State::INITIALIZING); }
  void child_shutdown() noexcept { change_child_state(State::SHUT_DOWNING); }

  void on_child_terminated(state_message_t* s) noexcept
  {
    switch (policy_)
    {
    case ShutdownPolicy::REBOOT:
      for (auto* child : childs_)
      {
        if (&child->message_ == s)
        {
          child->message_.set_state(State::INITIALIZING);
          child->message_.set_address(child);
          send(&child->message_);
          break;
        }
      }
      break;

    case ShutdownPolicy::CASCADE:
      message_.set_state(State::SHUT_DOWNING);
      message_.set_address(this);
      send(&message_);
      break;
    }
  }
};

} // namespace tartigrada
