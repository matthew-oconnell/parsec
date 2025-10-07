// Lightweight public header for the parsec library
#pragma once

namespace ps {

// Simple function used by tests and users
[[nodiscard]] inline int add(int a, int b) noexcept {
    return a + b;
}

} // namespace ps
