find_package(Catch2 REQUIRED)

add_executable(tartigrada_tests
    test_type_id.cpp
    test_queue.cpp
    test_actor.cpp
    test_timer.cpp
)

target_link_libraries(tartigrada_tests PRIVATE
    tartigrada
    Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(tartigrada_tests)
