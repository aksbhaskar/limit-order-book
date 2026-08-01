#include <iostream>

#include "lob/version.hpp"

// Minimal entry point. The executable currently only reports the build version;
// it exists to keep the project building end-to-end while the engine is
// developed incrementally.
int main() {
    std::cout << "limit-order-book " << lob::kVersionString << '\n';
    return 0;
}
