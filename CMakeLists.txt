cmake_minimum_required(VERSION 3.15)
project(AetherRuntime VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# Main Executable
add_executable(aether_runtime src/main.cpp)

# Test Suite
add_executable(test_suite tests/test_suite.cpp)

# Benchmarks
add_executable(benchmark benchmarks/benchmark.cpp)

enable_testing()
add_test(NAME AllUnitTests COMMAND test_suite)
