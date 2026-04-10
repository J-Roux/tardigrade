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

struct handler_t : handler_ptr
{
  using fn_t = void(*)(actor_base_t*, message_base_t*,
                       tartigrada::size_t, actor_base_t*) noexcept;

  actor_base_t* actor;
  fn_t          fn;

  handler_t() = default;
  handler_t(actor_base_t* actor, fn_t fn) : actor{actor}, fn{fn} {}

  void call(message_base_t* data,
            tartigrada::size_t id,
            actor_base_t* addr) noexcept override
  {
    fn(actor, data, id, addr);
  }

  template<auto MemFn>
  static fn_t trampoline() noexcept { return &trampoline_impl<MemFn>; }

private:
  template<auto MemFn>
  static void trampoline_impl(actor_base_t* actor, message_base_t* data,
                              tartigrada::size_t id, actor_base_t* addr) noexcept
  {
    using traits = handler_traits<decltype(MemFn)>;
    using A = typename traits::actor_t;
    using M = typename traits::message_t;
    if (id == M::type_id && ((addr == nullptr) || (addr == actor)))
      (static_cast<A*>(actor)->*MemFn)(static_cast<M*>(data));
  }
};

template<tartigrada::size_t N>
struct handler_pack_t
{
  handler_t items[N];
};

} // namespace tartigrada
