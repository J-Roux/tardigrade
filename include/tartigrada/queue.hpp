#pragma once

#include "platform.hpp"

namespace tartigrada
{

template<class Node>
class queue
{
  Node* head = nullptr;
  Node* tail = nullptr;
  tartigrada::size_t size = 0;

public:
  [[nodiscard]] constexpr bool empty() const noexcept
  {
    return size == 0;
  }

  void push_back(Node* n) noexcept
  {
    n->ptr = nullptr;
    if (tail) tail->ptr = n;
    else      head      = n;
    tail = n;
    ++size;
  }

  void pop_front() noexcept
  {
    head = head->ptr;
    if (!head) tail = nullptr;
    --size;
  }

  [[nodiscard]] constexpr tartigrada::size_t length() const noexcept
  {
    return size;
  }

  [[nodiscard]] constexpr Node* front() noexcept
  {
    return head;
  }

  struct iterator
  {
    Node* current;

    [[nodiscard]] constexpr Node* operator*()  const noexcept { return current; }
    constexpr iterator&           operator++()       noexcept { current = current->ptr; return *this; }
    [[nodiscard]] constexpr bool  operator!=(const iterator& o) const noexcept { return current != o.current; }
  };

  constexpr iterator begin() noexcept { return { head }; }
  constexpr iterator end()   noexcept { return { nullptr }; }
};

} // namespace tartigrada
