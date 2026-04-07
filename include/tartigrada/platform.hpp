#pragma once


 #include <stddef.h>
namespace tartigrada { using size_t = ::size_t; }


namespace tartigrada
{

// Default no-op critical section used by dispatch()/step()/run().
// Replace with your platform type:
//
//   struct avr_cs_t {
//       unsigned char sreg_;
//       avr_cs_t()  noexcept : sreg_{SREG} { cli(); }
//       ~avr_cs_t() noexcept { SREG = sreg_; }
//   };
//   supervisor.run<avr_cs_t>();
struct empty_critical_section_t
{
    empty_critical_section_t()  noexcept = default;
    ~empty_critical_section_t() noexcept = default;
};

} // namespace tartigrada
