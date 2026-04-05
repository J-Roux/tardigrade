#pragma once

// <cstddef> is unavailable on bare-metal AVR (no libstdc++ headers).
// <stddef.h> is always available via avr-libc / freestanding GCC.
#ifdef __AVR__
  #include <stddef.h>
  namespace tartigrada { using size_t = ::size_t; }
#else
  #include <cstddef>
  namespace tartigrada { using size_t = std::size_t; }
#endif
