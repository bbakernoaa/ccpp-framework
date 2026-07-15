# CCPP Framework C++ Modernization - CMake Build Configuration
# Phase 1: Native Object-Oriented C++ orchestration driver

cmake_minimum_required(VERSION 3.14)

option(CCPP_BUILD_OFFLINE "Build libccpp_offline with nanobind Python bindings" OFF)

# --- Vendored mdspan header ---
# Kokkos reference implementation of C++23 mdspan (single-header, C++17 compatible)
add_library(ccpp_mdspan INTERFACE)
target_include_directories(ccpp_mdspan INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vendor/mdspan>)
target_compile_features(ccpp_mdspan INTERFACE cxx_std_17)

# --- Core C++ framework (always built) ---
add_library(ccpp_core STATIC
    src/ccpp_pipeline_element.cpp
    src/ccpp_group.cpp
    src/ccpp_suite.cpp
    src/ccpp_constituent_registry.cpp
    src/ccpp_framework_orchestrator.cpp
    src/ccpp_bridge.cpp
)
target_link_libraries(ccpp_core PUBLIC ccpp_mdspan)
target_include_directories(ccpp_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(ccpp_core PUBLIC cxx_std_17)

# --- Tests (optional, enabled with CCPP_BUILD_TESTS) ---
option(CCPP_BUILD_TESTS "Build CCPP framework C++ unit and property tests" OFF)
if(CCPP_BUILD_TESTS)
    add_subdirectory(tests)
endif()
