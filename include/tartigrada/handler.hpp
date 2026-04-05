#pragma once

#include "handler_traits.hpp"
#include "message.hpp"
#include "node.hpp"
#include "platform.hpp"

namespace tartigrada
{

struct handler_ptr : public node<handler_ptr>
{
  // id and addr are snapshotted by dispatch() before the handler loop
  // so mutations a handler makes to the message don't affect other handlers.
  virtual void call(message_base_t* data,
                    tartigrada::size_t id,
                    actor_base_t* addr) noexcept = 0;
};

template<class Handler>
struct handler_t : handler_ptr
{
  actor_base_t* actor;
  Handler handler;

  handler_t(actor_base_t* actor, Handler&& handler_)
      : actor{ actor }, handler{ handler_ } {}

  void call(message_base_t* data,
            tartigrada::size_t id,
            actor_base_t* addr) noexcept override
  {
    if (id == final_message_t::type_id &&
        ((addr == nullptr) || (addr == actor)))
    {
      auto* final_message = static_cast<final_message_t*>(data);
      auto* final_obj     = static_cast<final_actor_t*>(actor);
      (*final_obj.*handler)(final_message);
    }
  }

private:
  using traits        = handler_traits<Handler>;
  using final_message_t = typename traits::message_t;
  using final_actor_t   = typename traits::actor_t;
};

} // namespace tartigrada
