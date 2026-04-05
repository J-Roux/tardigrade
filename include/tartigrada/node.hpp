#pragma once

namespace tartigrada
{

template<class Node> class queue; // forward declare for friend

template<class T>
struct node
{
  friend class queue<T>;
private:
  T* ptr = nullptr;
};

} // namespace tartigrada
