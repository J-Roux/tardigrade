#pragma once

#include "handler.hpp"
#include "platform.hpp"
#include "queue.hpp"

namespace tartigrada
{

struct actor_base_t;


class environment_t
{
public:
  environment_t() = default;

  void post(message_base_t* msg) noexcept
  {
    messageQueue.push_back(msg);
  }

  void subscribe(handler_ptr* h) noexcept
  {
    handles.push_back(h);
  }

  // Dispatch one ready message from the queue.
  // CS is a RAII critical-section type that disables interrupts around
  // queue accesses so ISRs may safely call post() concurrently.
  // The default (empty_critical_section_t) is a no-op for host builds.
  template<class CS = empty_critical_section_t>
  [[nodiscard]] bool dispatch() noexcept
  {
    // Scan at most the current queue length so not-ready messages
    // moved to the back don't cause infinite spinning.
    const auto pending = messageQueue.length();
    for (tartigrada::size_t i = 0; i < pending; ++i)
    {
      message_base_t* msg;
      { 
        CS cs; 
        msg = messageQueue.front(); 
        messageQueue.pop_front(); 

        if (!msg->is_ready())
        {
          messageQueue.push_back(msg); // move to back, try next
          continue;
        }
      }
      const auto  id   = msg->get_id();
      auto* const addr = msg->get_address();
      for (auto* h : handles)
        h->call(msg, id, addr);
      return true;
    }
    return false;
  }

  [[nodiscard]] constexpr actor_base_t* supervisor() const noexcept { return supervisor_; }
  constexpr void set_supervisor(actor_base_t* s)          noexcept { supervisor_ = s; }

private:
  queue<message_base_t> messageQueue;
  queue<handler_ptr>    handles;
  actor_base_t*         supervisor_ = nullptr;
};

} // namespace tartigrada
