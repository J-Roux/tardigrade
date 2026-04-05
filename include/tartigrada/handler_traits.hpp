#pragma once

namespace tartigrada
{

template <typename T> struct handler_traits {};

template <typename A, typename M>
struct handler_traits<void (A::*)(M*) noexcept>
{
  using actor_t = A;
  using message_t = M;
};

} // namespace tartigrada
