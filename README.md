parsec
======

Small example C++ library 'parsec' (namespace ps::) with Catch2 unit tests and modern CMake.

Build (out-of-source):

```bash
cmake -S . -B build -DPARSEC_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The public headers are under `include/ps` and the library target is `parsec`.
