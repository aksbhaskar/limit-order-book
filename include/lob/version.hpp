#pragma once

#include <string_view>

namespace lob {

// Semantic version of the library. Kept in sync with the CMake project version.
inline constexpr int kVersionMajor = 0;
inline constexpr int kVersionMinor = 1;
inline constexpr int kVersionPatch = 0;

inline constexpr std::string_view kVersionString = "0.1.0";

}  // namespace lob
