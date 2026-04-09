if(NOT CMAKE_CROSSCOMPILING)
  add_executable(ping_pong ping_pong.cpp)
  target_link_libraries(ping_pong PRIVATE tartigrada Threads::Threads)
endif()

# ping_pong_bare has no platform headers — compiles on both host and AVR.
add_executable(ping_pong_bare ping_pong_bare.cpp)
target_link_libraries(ping_pong_bare PRIVATE tartigrada)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "avr")
  # AVR compile/link options shared by all firmware targets.
  set(_avr_compile_opts
      -mmcu=${AVR_MCU}
      -DF_CPU=16000000UL
      -Os
      -fno-exceptions
      -fno-rtti
      -ffunction-sections
      -fdata-sections
      # Allow firmware to include <avr/avr_mcu_section.h> from simavr,
      # which embeds MCU name + frequency in the ELF for sim_runner.
      -I/usr/include/simavr
  )
  set(_avr_link_opts
      -mmcu=${AVR_MCU}
      -Wl,--gc-sections
  )

  # ping_pong_bare — add AVR-specific flags to the already-defined target.
  target_compile_options(ping_pong_bare PRIVATE ${_avr_compile_opts})
  target_link_options(ping_pong_bare    PRIVATE ${_avr_link_opts})
  add_custom_command(TARGET ping_pong_bare POST_BUILD
      COMMAND avr-size --format=avr --mcu=${AVR_MCU} $<TARGET_FILE:ping_pong_bare>
      VERBATIM
  )

  # Helper: define an AVR-only firmware target.
  function(add_avr_firmware target src)
    add_executable(${target} ${src})
    target_link_libraries(${target} PRIVATE tartigrada)
    target_compile_options(${target} PRIVATE ${_avr_compile_opts})
    target_link_options(${target}    PRIVATE ${_avr_link_opts})
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND avr-size --format=avr --mcu=${AVR_MCU} $<TARGET_FILE:${target}>
        VERBATIM
    )
  endfunction()

  add_avr_firmware(arduino_watchdog arduino_watchdog.cpp)
endif()

# sim_<name> — host targets that run an AVR ELF through sim_runner.
# Requires: sim_runner built (host tree) + AVR ELF built (build-avr/ tree).
if(NOT CMAKE_CROSSCOMPILING)
  set(_avr_build_dir "${CMAKE_SOURCE_DIR}/build-avr/examples")

  # add_sim_target(name [SIM_USEC <usec>] [MCU <mcu>] [FREQ <hz>])
  # MCU and FREQ override the .mmcu ELF section (useful when --gc-sections strips it).
  function(add_sim_target name)
    cmake_parse_arguments(ARG "" "SIM_USEC;MCU;FREQ" "" ${ARGN})
    if(NOT ARG_SIM_USEC)
      set(ARG_SIM_USEC 30000000)   # 30 s default
    endif()

    set(_extra_args "")
    if(ARG_MCU)
      list(APPEND _extra_args "--mcu=${ARG_MCU}")
    endif()
    if(ARG_FREQ)
      list(APPEND _extra_args "--freq=${ARG_FREQ}")
    endif()

    add_custom_target(sim_${name}
        COMMAND sim_runner
            "${_avr_build_dir}/${name}"
            "${ARG_SIM_USEC}"
            ${_extra_args}
        DEPENDS sim_runner
        COMMENT "Simulating ${name} (${ARG_SIM_USEC} µs of simulated AVR time)"
        VERBATIM
    )
  endfunction()

  add_sim_target(arduino_watchdog SIM_USEC 30000000 MCU atmega328p FREQ 16000000)
endif()
