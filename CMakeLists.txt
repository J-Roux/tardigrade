cmake_minimum_required(VERSION 3.16)
project(tartigrada CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(tartigrada INTERFACE)
target_include_directories(tartigrada INTERFACE include)

if(NOT CMAKE_CROSSCOMPILING)
  find_package(Threads REQUIRED)
  enable_testing()
  add_subdirectory(tests)
  if(TARTIGRADA_WITH_SIMAVR)
    add_subdirectory(tools)
  endif()
endif()

add_subdirectory(examples)
