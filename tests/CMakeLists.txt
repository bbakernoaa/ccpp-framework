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

FetchContent_MakeAvailable(googletest rapidcheck)

enable_testing()

# --- Property test: mdspan layout preservation (Property 2) ---
add_executable(test_mdspan_property
    test_mdspan_layout_property.cpp
)
target_link_libraries(test_mdspan_property PRIVATE
    ccpp_mdspan
    GTest::gtest_main
    rapidcheck
)
target_compile_features(test_mdspan_property PRIVATE cxx_std_17)
target_include_directories(test_mdspan_property PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

include(GoogleTest)
gtest_discover_tests(test_mdspan_property)

# --- Property test: submdspan slicing (Property 9) ---
add_executable(test_submdspan_slicing_property
    test_submdspan_slicing.cpp
)
target_link_libraries(test_submdspan_slicing_property PRIVATE
    ccpp_mdspan
    GTest::gtest_main
    rapidcheck
)
target_compile_features(test_submdspan_slicing_property PRIVATE cxx_std_17)
target_include_directories(test_submdspan_slicing_property PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

gtest_discover_tests(test_submdspan_slicing_property)
