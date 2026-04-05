#pragma once

#include "platform.hpp"

namespace tartigrada
{

template<class Node>
class queue
{
  Node* tail = nullptr;
  tartigrada::size_t size = 0;

public:
  [[nodiscard]] constexpr bool empty() const noexcept
  {
    return size == 0;
  }

  void push_front(Node* n) noexcept
  {
    auto** temp = &tail;
    for (tartigrada::size_t i = 0; i < size; i++)
    {
      temp = &((*temp)->ptr);
    }
    *temp = n;
    size++;
  }

  void pop_front() noexcept
  {
    tail = tail->ptr;
    size--;
  }

  [[nodiscard]] constexpr tartigrada::size_t length() const noexcept
  {
    return size;
  }

  [[nodiscard]] constexpr Node* front() noexcept
  {
    return tail;
  }

  struct iterator
  {
    Node* current;

    [[nodiscard]] constexpr Node* operator*()  const noexcept { return current; }
    constexpr iterator&           operator++()       noexcept { current = current->ptr; return *this; }
    [[nodiscard]] constexpr bool  operator!=(const iterator& o) const noexcept { return current != o.current; }
  };

  constexpr iterator begin() noexcept { return { tail }; }
  constexpr iterator end()   noexcept { return { nullptr }; }
};

} // namespace tartigrada
