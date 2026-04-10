find_package(PkgConfig REQUIRED)
if(SIMAVR_INSTALL_PREFIX)
  set(ENV{PKG_CONFIG_PATH} "${SIMAVR_INSTALL_PREFIX}/lib/pkgconfig")
endif()
pkg_check_modules(SIMAVR REQUIRED simavr)

add_executable(sim_runner sim_runner.cpp)
target_include_directories(sim_runner PRIVATE
    $<IF:$<BOOL:${SIMAVR_INSTALL_PREFIX}>,${SIMAVR_INSTALL_PREFIX}/include,${SIMAVR_INCLUDE_DIRS}>
)
target_link_directories(sim_runner PRIVATE ${SIMAVR_LIBRARY_DIRS})
target_link_libraries(sim_runner PRIVATE ${SIMAVR_LIBRARIES} elf)
