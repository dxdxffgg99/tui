cmake_minimum_required(VERSION 3.10)
project(tui C)

# Options
option(BUILD_TEST "Build the test/example executable" ON)

# Compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/inc)

if(BUILD_TEST)
  add_executable(tui_test test.c)
  target_include_directories(tui_test PRIVATE ${CMAKE_SOURCE_DIR}/inc)
  if(MSVC)
    target_compile_options(tui_test PRIVATE /W4 /WX)
    target_compile_options(tui_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(tui_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  install(TARGETS tui_test RUNTIME DESTINATION bin)
endif()

# Basic test target
enable_testing()
