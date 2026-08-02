include(FetchContent)

# doctest: a lightweight, single-header unit-testing framework.
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
    GIT_SHALLOW    TRUE
)
# doctest's bundled CMake declares a very old minimum policy version that newer
# CMake (>= 4.0) rejects. Raise the floor just for this dependency so it keeps
# configuring; our own project still targets the policies of CMake 3.20+.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

FetchContent_MakeAvailable(doctest)

add_executable(unit_tests
    doctest_main.cpp
    test_sanity.cpp
    test_types.cpp
    test_price.cpp
    test_quantity.cpp
    test_order.cpp
    test_price_level.cpp
    test_order_book.cpp
    test_order_fill.cpp
    test_matching_engine.cpp
)
target_link_libraries(unit_tests PRIVATE lob lob_warnings doctest::doctest)

add_test(NAME unit_tests COMMAND unit_tests)
