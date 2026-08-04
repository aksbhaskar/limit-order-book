// Single translation unit that compiles nanobench's implementation. Keeping it
// isolated lets every other benchmark file include the header cheaply.
#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>
