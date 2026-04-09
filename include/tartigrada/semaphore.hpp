#pragma once

#include "message.hpp"
#include "platform.hpp"

namespace tartigrada
{

// Counting semaphore. Tracks a resource count; try_acquire() decrements
// it (returns false when already zero), release() increments it.
class semaphore_t
{
    tartigrada::size_t count_;
public:
    explicit constexpr semaphore_t(tartigrada::size_t initial) noexcept
        : count_{ initial } {}

    [[nodiscard]] constexpr tartigrada::size_t count() const noexcept { return count_; }

    // Decrements the count. Pre-condition: count() > 0 (use after is_ready()).
    constexpr void acquire() noexcept { --count_; }

    // Returns true and decrements the count, or returns false if count == 0.
    [[nodiscard]] constexpr bool try_acquire() noexcept
    {
        if (count_ == 0) return false;
        --count_;
        return true;
    }

    constexpr void release() noexcept { ++count_; }

    // Resets the count to the given value (default 0). Use instead of a drain
    // loop in contexts where the new count is known (e.g. embedded shutdown).
    constexpr void reset(tartigrada::size_t val = 0) noexcept { count_ = val; }
};

// Message base that defers dispatch until the bound semaphore has a
// non-zero count.  The handler is responsible for calling try_acquire()
// when it consumes the resource and release() when it is done.
//
//   struct my_msg_t : semaphore_message_t<my_msg_t> {};
//   my_msg_t msg;
//   msg.bind(some_semaphore);
template<typename Derived>
class semaphore_message_t : public message_t<Derived>
{
    semaphore_t* sem_ = nullptr;
public:
    constexpr void bind(semaphore_t& s) noexcept { sem_ = &s; }

    [[nodiscard]] bool is_ready() noexcept override
    {
        return !sem_ || sem_->count() > 0;
    }
};

} // namespace tartigrada
