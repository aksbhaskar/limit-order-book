include(FetchContent)

# nanobench: a single-header, dependency-free microbenchmarking library that
# reports ns/op, ops/s, and a noise estimate.
FetchContent_Declare(
    nanobench
    GIT_REPOSITORY https://github.com/martinus/nanobench.git
    GIT_TAG        v4.3.11
    GIT_SHALLOW    TRUE
)
# nanobench's bundled CMake declares an old minimum policy version that newer
# CMake rejects; raise the floor just for this dependency (as we do for doctest).
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
FetchContent_MakeAvailable(nanobench)

add_executable(lob_benchmarks
    nanobench_impl.cpp
    bench_main.cpp
)
target_link_libraries(lob_benchmarks PRIVATE lob nanobench)

# Benchmarks are only meaningful optimized; force -O2/-O3 for this target even in
# a Debug configuration, without disturbing the rest of the build.
if(NOT MSVC)
    target_compile_options(lob_benchmarks PRIVATE -O3)
endif()
