#pragma once

#include "handler.hpp"
#include "queue.hpp"

namespace tartigrada
{

struct actor_base_t;

struct enviroment_t
{
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
    if (messageQueue.empty()) return false;
    auto* msg        = messageQueue.front();
    messageQueue.pop_front();
    const auto  id   = msg->get_id();
    auto* const addr = msg->get_address();
    for (auto* h : handles)
      h->call(msg, id, addr);
    return true;
  }

  [[nodiscard]] constexpr actor_base_t* top() const noexcept { return top_; }
  constexpr void set_top(actor_base_t* s)          noexcept { top_ = s; }

private:
  queue<message_base_t> messageQueue;
  queue<handler_ptr>    handles;
  actor_base_t*         top_ = nullptr;
};

} // namespace tartigrada
