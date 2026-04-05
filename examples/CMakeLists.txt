if(NOT CMAKE_CROSSCOMPILING)
  add_executable(ping_pong ping_pong.cpp)
  target_link_libraries(ping_pong PRIVATE tartigrada Threads::Threads)
endif()

add_executable(ping_pong_bare ping_pong_bare.cpp)
target_link_libraries(ping_pong_bare PRIVATE tartigrada)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "avr")
  target_compile_options(ping_pong_bare PRIVATE
      -mmcu=${AVR_MCU}
      -Os
      -fno-exceptions
      -fno-rtti
      -ffunction-sections
      -fdata-sections
  )
  target_link_options(ping_pong_bare PRIVATE
      -mmcu=${AVR_MCU}
      -Wl,--gc-sections
  )
  add_custom_command(TARGET ping_pong_bare POST_BUILD
      COMMAND avr-size --format=avr --mcu=${AVR_MCU} $<TARGET_FILE:ping_pong_bare>
      VERBATIM
  )
endif()
