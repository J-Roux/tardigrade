#pragma once

#include "handler.hpp"
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
    messageQueue.push_front(msg);
  }

  void subscribe(handler_ptr* h) noexcept
  {
    handles.push_front(h);
  }

  [[nodiscard]] bool dispatch() noexcept
  {
    // Scan at most the current queue length so not-ready messages
    // moved to the back don't cause infinite spinning.
    const auto pending = messageQueue.length();
    for (tartigrada::size_t i = 0; i < pending; ++i)
    {
      auto* msg = messageQueue.front();
      messageQueue.pop_front();
      if (!msg->is_ready())
      {
        messageQueue.push_front(msg); // move to back, try next
        continue;
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
