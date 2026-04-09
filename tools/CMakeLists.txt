find_package(PkgConfig REQUIRED)
pkg_check_modules(SIMAVR REQUIRED simavr)

add_executable(sim_runner sim_runner.cpp)
target_compile_options(sim_runner PRIVATE ${SIMAVR_CFLAGS})
target_link_libraries(sim_runner PRIVATE ${SIMAVR_LIBRARIES} elf)
