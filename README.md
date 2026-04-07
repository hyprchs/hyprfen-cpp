# hyprfen-cpp

`hyprfen-cpp` is a reusable C++ implementation of the `hyprfen` chess FEN codec.

It encodes standard chess FENs into a compact binary representation and decodes
them back into canonical standard FEN strings. The bitstream matches the Python
reference implementation in [`hyprfen`](https://github.com/hyprchs/hyprfen).

## Status

- Library API: implemented
- CMake package/export metadata: implemented
- Golden compatibility tests against the Python reference bitstream: implemented

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## Public API

```cpp
#include <hyprfen/hyprfen.hpp>

std::vector<std::uint8_t> blob = hyprfen::encode_fen(
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
);
std::string fen = hyprfen::decode_fen(blob);
hyprfen::EncodeStats stats = hyprfen::encoding_stats(fen);
```

## Consumption

Installed-package flow:

```cmake
find_package(hyprfen CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE hyprfen::hyprfen)
```

`FetchContent` flow:

```cmake
include(FetchContent)

FetchContent_Declare(
  hyprfen
  GIT_REPOSITORY git@github.com:hyprchs/hyprfen-cpp.git
  GIT_TAG main
)
FetchContent_MakeAvailable(hyprfen)

target_link_libraries(your_target PRIVATE hyprfen::hyprfen)
```

## Notes

- This codec supports standard chess FENs with standard `KQkq` castling notation.
- The decoder emits canonical standard FEN strings.
- The library is intentionally dependency-free beyond the C++ standard library.

For Python-first workflows, use the reference implementation in `hyprfen`.
