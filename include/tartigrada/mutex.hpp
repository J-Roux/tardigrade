#pragma once

#include "message.hpp"

namespace tartigrada
{

// Binary lock. The actor that calls lock() owns the mutex until it calls unlock().
class mutex_t
{
    bool locked_ = false;
public:
    [[nodiscard]] constexpr bool is_locked() const noexcept { return locked_; }
    constexpr void lock()   noexcept { locked_ = true;  }
    constexpr void unlock() noexcept { locked_ = false; }
};

// Message base that defers dispatch until the bound mutex is free.
// Derive your message from this and call bind() to attach the mutex:
//
//   struct my_msg_t : mutex_message_t<my_msg_t> {};
//   my_msg_t msg;
//   msg.bind(some_mutex);
template<typename Derived>
class mutex_message_t : public message_t<Derived>
{
    mutex_t* mutex_ = nullptr;
public:
    constexpr void bind(mutex_t& m) noexcept { mutex_ = &m; }

    [[nodiscard]] bool is_ready() noexcept override
    {
        return !mutex_ || !mutex_->is_locked();
    }
};

} // namespace tartigrada
