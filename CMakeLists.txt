cmake_minimum_required(VERSION 3.20)
project(os_simulators VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SCHED_BUILD_TESTS "Build the GoogleTest suite" ON)
option(SCHED_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(SCHED_WERROR "Treat warnings as errors" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(SCHED_BUILD_TESTS)
  enable_testing()
endif()
add_subdirectory(scheduler)
