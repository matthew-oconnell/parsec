parsec
======

Small example C++ library 'parsec' (namespace ps::) with Catch2 unit tests and modern CMake.

Repository layout (refactored):
- src/        : library sources and public headers under `src/include/ps`
- test/       : unit tests and test CMake
- examples/   : example JSON files used by tests
- build/      : out-of-source CMake build directory (ignored in Git)

Build (out-of-source):

```bash
cmake -S . -B build -DPARSEC_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The library target is `parsec` and the public headers are under `src/include/ps`.
