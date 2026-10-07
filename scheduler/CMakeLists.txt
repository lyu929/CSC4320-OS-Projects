add_library(sched
  src/engine.cpp
  src/policies.cpp
  src/io.cpp
  src/workload.cpp
)
add_library(sched::sched ALIAS sched)
target_include_directories(sched PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(sched PUBLIC cxx_std_20)

if(MSVC)
  set(SCHED_WARNINGS /W4 /permissive-)
  if(SCHED_WERROR)
    list(APPEND SCHED_WARNINGS /WX)
  endif()
else()
  set(SCHED_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wnon-virtual-dtor
                     -Wold-style-cast -Woverloaded-virtual)
  if(SCHED_WERROR)
    list(APPEND SCHED_WARNINGS -Werror)
  endif()
endif()
target_compile_options(sched PRIVATE ${SCHED_WARNINGS})

if(SCHED_SANITIZE AND NOT MSVC)
  set(SAN -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
  target_compile_options(sched PUBLIC ${SAN})
  target_link_options(sched PUBLIC ${SAN})
endif()

foreach(app schedsim schedbench)
  add_executable(${app} apps/${app}.cpp)
  target_link_libraries(${app} PRIVATE sched::sched)
  target_compile_options(${app} PRIVATE ${SCHED_WARNINGS})
endforeach()

if(SCHED_BUILD_TESTS)
  include(FetchContent)
  # Offline builds: cmake -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/to/googletest
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  add_executable(sched_tests
    tests/test_policies.cpp
    tests/test_properties.cpp
    tests/test_io.cpp
    tests/test_workload.cpp)
  target_link_libraries(sched_tests PRIVATE sched::sched GTest::gtest_main)
  target_compile_definitions(sched_tests PRIVATE
    SCHED_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    SCHED_LEGACY_DIR="${PROJECT_SOURCE_DIR}/legacy/project1-cpu-scheduling")
  include(GoogleTest)
  gtest_discover_tests(sched_tests)
endif()
