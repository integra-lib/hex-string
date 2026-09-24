# hex-string

Hex text to bytes and back, into a buffer you own — no allocation, no exceptions, all `constexpr`.

Part of [hwlib](https://github.com/integra-lib) — architecture-independent C++20
components shared between firmware projects. Header-only,
no exceptions, no RTTI.

## Use it

```bash
git submodule add git@github.com:integra-lib/hex-string.git external/hwlib/hex-string
```

```cmake
add_subdirectory(external/hwlib/hex-string)
target_link_libraries(app PRIVATE Hwlib::hex_string)
```

```cpp
#include <hwlib/utilities/hex_string.hpp>
```

Each component carries its own include directory, so this header stays unreachable
until the component is linked: a forgotten dependency is a compile error rather than
a build that happens to work.

## Formatting

```cpp
// Sized with HexCharCount() plus one and value-initialised, so what was written is
// also a C string — no separate terminator step, and nothing to dangle in a log call.
std::array<char, hwlib::utilities::HexCharCount(sizeof(mac)) + 1U> text{};

// A MAC comes off the radio least significant byte first, the reverse of how it is
// written down, which is what BytesToHexReversed is for.
if (hwlib::utilities::BytesToHexReversed(mac, text).has_value())
{
    LOG_INF("peer %s", text.data());
}
```

`BytesToHex` writes lower-case hex, two characters per byte, leading zero included.
Both return the number of characters written, or nothing — having written nothing —
if the buffer is too small.

## Parsing

```cpp
std::array<std::uint8_t, 6> mac{};
const auto count = hwlib::utilities::HexToBytes(text, mac);
if (!count.has_value())
{
    return Reject();   // odd length, a character that is not a hex digit, or too long
}
```

Malformed input is a value, not an exception: the library is built for firmware where
exceptions are off, and the string usually came off a wire, where being malformed is
ordinary. Nothing is written unless the whole input parses, so a caller that ignored
the result cannot act on half a value.

Both directions are `constexpr`, so a fixed value costs nothing at run time.

## Coming from `hexstrconv`

This component arrived as two copies that had drifted — a138-ble-gateway's
`firmware/lib/utils/hexstrconv` and scale's `components/utils/hexstrconv` — and each
had caught a defect the other had not:

* scale's `FromHexStr` accepted `"0x"` as a single zero byte. `std::from_chars`
  reports success on a partial parse, and neither copy checked that it had consumed
  both characters; a138's copy only escaped it by pre-scanning with `isxdigit`.
* that pre-scan passed a possibly negative `char` to `std::isxdigit`, which is
  undefined for anything but `unsigned char` values and `EOF`. A byte above 0x7F in
  the input is enough.

Both are gone here, and both are pinned by a test. What else changed:

* `std::vector<std::uint8_t>` and `std::string` returns became a caller's buffer plus
  a written count, so the conversion works where the heap does not;
* the three `throw`s became `std::optional`;
* `ToHexStr` and `ToHexStrInverted` became `BytesToHex` and `BytesToHexReversed` —
  the a138 copy carried a `// TODO: fix mac -> string presentation` next to the
  inverted one, and reversed byte order is the presentation, not a thing to fix;
* `FromStr2Bytes` was not carried over. It converts characters to bytes and has
  nothing to do with hex — it is `std::vector<std::uint8_t>{in.begin(), in.end()}`
  under a name that suggests otherwise.
* `IntToHexStr` was not carried over either: it formats through `std::stringstream`,
  which is the one thing a firmware binary cannot afford to link.

## Versioning

Every component is released on its own, tagged `vX.Y.Z`. Pre-1.0, a minor release may
break the API, which is why dependants accept a single minor.

```bash
git -C external/hwlib/hex-string fetch --tags
git -C external/hwlib/hex-string checkout v0.2.0
git add external/hwlib/hex-string && git commit -m "build: bump hex-string to v0.2.0"
```

## In a consumer's CI

The component is an ordinary submodule, so the build needs it checked out. On GitLab
that means `GIT_SUBMODULE_STRATEGY: normal` (or `recursive`) on every job that builds —
not only on the ones that run unit tests.

## Develop it

```bash
git submodule update --init          # ci-shared, needed by pre-commit
cmake -S . -B build && cmake --build build -j && ctest --test-dir build
```

Tests are built only when this repository is the top-level project, so a consumer
never builds them and never fetches GoogleTest.

The style configs are symlinks into the `ci-shared` submodule, and the pipeline comes
from the same place. On GitHub this repository carries a self-contained build-and-test
workflow instead: a workflow token cannot read another private repository, so neither
a shared workflow nor the submodule is reachable there. The shared setup is what
GitLab will use.
