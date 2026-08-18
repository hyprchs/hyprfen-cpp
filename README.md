# hyprfen-cpp

`hyprfen-cpp` is a C++ implementation of the Python package [`hyprfen`](https://github.com/hyprchs/hyprfen). From its readme:

> `hyprfen` stores standard chess FENs in about **64.9% fewer bits** than raw FEN strings on a 100,000-position real-game sample from Lichess.
> 
> It is a compact, reversible binary codec for standard chess positions. You give it a FEN string, it gives you bytes, and `decode_fen()` returns the exact original FEN.

This C++ implementation checks byte-for-byte compatibility with the Python implementation across 100,000 unique FENs from the Lichess January 2013 standard-rated dump. That means you can encode with `hyprfen` and decode with `hyprfen-cpp` or vice versa to get back the same FEN.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

CI also runs the 100,000-case bitstream compatibility test. To run it locally:

```bash
curl --fail --location --output /tmp/hyprfen-bitstreams_lichess-2013-01_100k.tsv \
  https://github.com/hyprchs/hyprfen/releases/download/bitstream-compatibility-v1/hyprfen-bitstreams_lichess-2013-01_100k.tsv
echo '2a20ca230d172db191d9981734a2e8d99a97ac9b8747604e5e40580a1650ba7f  /tmp/hyprfen-bitstreams_lichess-2013-01_100k.tsv' | shasum -a 256 -c -
cmake -S . -B build -DHYPRFEN_BITSTREAM_VECTORS=/tmp/hyprfen-bitstreams_lichess-2013-01_100k.tsv
ctest --test-dir build -R hyprfen_bitstream_compatibility --output-on-failure
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
