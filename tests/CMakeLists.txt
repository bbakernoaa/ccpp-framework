# CCPP Framework C++ Unit & Property-Based Tests
# Requires: GoogleTest, RapidCheck, C++17

cmake_minimum_required(VERSION 3.14)

# --- Dependencies via FetchContent ---
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

FetchContent_Declare(
    rapidcheck
    GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
    GIT_TAG        master
)

# Prevent overriding the parent project's compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# RapidCheck's GoogleTest integration header (rapidcheck/gtest.h, providing
# RC_GTEST_PROP) is only built when RC_ENABLE_GTEST is ON. Without this,
# RapidCheck builds successfully but every RC_GTEST_PROP-based test file
# fails with "rapidcheck/gtest.h file not found".
set(RC_ENABLE_GTEST ON CACHE BOOL "Build RapidCheck GTest integration" FORCE)
set(RC_ENABLE_CATCH OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest rapidcheck)

enable_testing()
include(GoogleTest)

# --- Property test: mdspan layout preservation (Property 2) ---
add_executable(test_mdspan_property test_mdspan_layout_property.cpp)
target_link_libraries(test_mdspan_property PRIVATE
    ccpp_mdspan GTest::gtest_main rapidcheck rapidcheck_gtest)
target_compile_features(test_mdspan_property PRIVATE cxx_std_17)
target_include_directories(test_mdspan_property PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include)
gtest_discover_tests(test_mdspan_property)

# --- Property test: submdspan slicing (Property 9) ---
add_executable(test_submdspan_slicing_property test_submdspan_slicing.cpp)
target_link_libraries(test_submdspan_slicing_property PRIVATE
    ccpp_mdspan GTest::gtest_main rapidcheck rapidcheck_gtest)
target_compile_features(test_submdspan_slicing_property PRIVATE cxx_std_17)
target_include_directories(test_submdspan_slicing_property PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include)
gtest_discover_tests(test_submdspan_slicing_property)

# --- Property test: Pipeline Execution Ordering (Property 4) ---
add_executable(test_pipeline_property test_pipeline_property.cpp)
target_link_libraries(test_pipeline_property PRIVATE
    ccpp_core GTest::gtest_main rapidcheck rapidcheck_gtest)
target_compile_features(test_pipeline_property PRIVATE cxx_std_17)
gtest_discover_tests(test_pipeline_property)

# --- Property test: Constituent Registry Round-Trip (Property 8) ---
add_executable(test_constituent_property test_constituent_property.cpp)
target_link_libraries(test_constituent_property PRIVATE
    ccpp_core GTest::gtest_main rapidcheck rapidcheck_gtest)
target_compile_features(test_constituent_property PRIVATE cxx_std_17)
gtest_discover_tests(test_constituent_property)

# --- Property test: Exception Containment at Language Boundary (Property 5) ---
add_executable(test_bridge_exception_property test_bridge_exception_property.cpp)
target_link_libraries(test_bridge_exception_property PRIVATE
    ccpp_core GTest::gtest_main rapidcheck rapidcheck_gtest)
target_compile_features(test_bridge_exception_property PRIVATE cxx_std_17)
gtest_discover_tests(test_bridge_exception_property)

# --- Property test: Successful Execution Returns Zero (Property 6) ---
add_executable(test_bridge_success_property test_bridge_success_property.cpp)
target_link_libraries(test_bridge_success_property PRIVATE
    ccpp_core GTest::gtest_main rapidcheck rapidcheck_gtest)
target_compile_features(test_bridge_success_property PRIVATE cxx_std_17)
gtest_discover_tests(test_bridge_success_property)

# --- Property test: Error Message Preservation (Property 7) ---
add_executable(test_bridge_errmsg_property test_bridge_errmsg_property.cpp)
target_link_libraries(test_bridge_errmsg_property PRIVATE
    ccpp_core GTest::gtest_main rapidcheck rapidcheck_gtest)
target_compile_features(test_bridge_errmsg_property PRIVATE cxx_std_17)
gtest_discover_tests(test_bridge_errmsg_property)

# --- Integration Test: Full Pipeline Wiring ---
add_executable(test_full_pipeline integration/test_full_pipeline.cpp)
target_link_libraries(test_full_pipeline PRIVATE ccpp_core GTest::gtest_main)
target_compile_features(test_full_pipeline PRIVATE cxx_std_17)
gtest_discover_tests(test_full_pipeline)

# --- Integration Test: Mixed Fortran/C++ Scheme Pipeline ---
add_executable(test_mixed_pipeline integration/test_mixed_pipeline.cpp)
target_link_libraries(test_mixed_pipeline PRIVATE ccpp_core GTest::gtest_main)
target_compile_features(test_mixed_pipeline PRIVATE cxx_std_17)
gtest_discover_tests(test_mixed_pipeline)

# --- Integration Test: Constituent Lifecycle ---
add_executable(test_constituent_lifecycle integration/test_constituent_lifecycle.cpp)
target_link_libraries(test_constituent_lifecycle PRIVATE ccpp_core GTest::gtest_main)
target_compile_features(test_constituent_lifecycle PRIVATE cxx_std_17)
gtest_discover_tests(test_constituent_lifecycle)

# --- Integration Test: Suite Wiring Fixes (group_name="all" dispatch,
#     phase-gated subcycle looping, auto-registered suite factories) ---
add_executable(test_suite_wiring_fixes integration/test_suite_wiring_fixes.cpp)
target_link_libraries(test_suite_wiring_fixes PRIVATE ccpp_core GTest::gtest_main)
target_compile_features(test_suite_wiring_fixes PRIVATE cxx_std_17)
gtest_discover_tests(test_suite_wiring_fixes)
