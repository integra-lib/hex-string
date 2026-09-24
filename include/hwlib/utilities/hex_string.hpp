#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace hwlib::utilities
{

/// Two hex characters per byte, everywhere below.
inline constexpr std::size_t HEX_CHARS_PER_BYTE = 2U;

/// How many characters `BytesToHex` writes for `byteCount` bytes. A buffer sized
/// with this plus one, value-initialised, is also a C string.
[[nodiscard]] constexpr std::size_t HexCharCount(std::size_t byteCount) noexcept
{
    return byteCount * HEX_CHARS_PER_BYTE;
}

namespace detail
{

/// The numeric value of one hex digit, or nothing if the character is not one.
/// Written out rather than delegated to std::isxdigit, which takes an int and is
/// undefined for a negative char — the defect this component arrived with.
[[nodiscard]] constexpr std::optional<std::uint8_t> HexDigit(char digit) noexcept
{
    if (digit >= '0' && digit <= '9')
    {
        return static_cast<std::uint8_t>(digit - '0');
    }
    if (digit >= 'a' && digit <= 'f')
    {
        return static_cast<std::uint8_t>(digit - 'a' + 10);
    }
    if (digit >= 'A' && digit <= 'F')
    {
        return static_cast<std::uint8_t>(digit - 'A' + 10);
    }
    return std::nullopt;
}

constexpr char NibbleToHex(std::uint8_t nibble) noexcept
{
    constexpr std::string_view DIGITS = "0123456789abcdef";
    return DIGITS[nibble & 0x0FU];
}

} // namespace detail

/// Parses a hex string into `out`, and returns how many bytes it wrote.
///
/// Returns nothing — having written nothing — when the input has an odd length,
/// holds a character that is not a hex digit, or does not fit in `out`. Malformed
/// input is a value, not an exception: the library is built for a firmware where
/// exceptions are off, and the string usually came off a wire, where being
/// malformed is ordinary.
///
/// Both cases are worth spelling out, because this component arrived with two
/// implementations that each got one of them wrong: one accepted "0x" as a single
/// zero byte (std::from_chars reports success on a partial parse, and neither copy
/// checked that it consumed both characters), and the other passed a possibly
/// negative char to std::isxdigit.
[[nodiscard]] constexpr std::optional<std::size_t> HexToBytes(std::string_view hex,
                                                              std::span<std::uint8_t> out) noexcept
{
    if ((hex.size() % HEX_CHARS_PER_BYTE) != 0U)
    {
        return std::nullopt;
    }
    const std::size_t byteCount = hex.size() / HEX_CHARS_PER_BYTE;
    if (byteCount > out.size())
    {
        return std::nullopt;
    }

    for (std::size_t i = 0U; i < byteCount; ++i)
    {
        const auto high = detail::HexDigit(hex[i * HEX_CHARS_PER_BYTE]);
        const auto low  = detail::HexDigit(hex[(i * HEX_CHARS_PER_BYTE) + 1U]);
        if (!high.has_value() || !low.has_value())
        {
            return std::nullopt;
        }
        out[i] = static_cast<std::uint8_t>((high.value() << 4U) | low.value());
    }
    return byteCount;
}

/// Formats bytes as lower-case hex into `out`, and returns how many characters it
/// wrote. Returns nothing, having written nothing, if `out` is too small — see
/// HexCharCount().
[[nodiscard]] constexpr std::optional<std::size_t> BytesToHex(std::span<const std::uint8_t> bytes,
                                                              std::span<char> out) noexcept
{
    if (HexCharCount(bytes.size()) > out.size())
    {
        return std::nullopt;
    }

    std::size_t written = 0U;
    for (const auto byte : bytes)
    {
        out[written++] = detail::NibbleToHex(static_cast<std::uint8_t>(byte >> 4U));
        out[written++] = detail::NibbleToHex(byte);
    }
    return written;
}

/// The same, least significant byte first. This is what prints a MAC address: the
/// radio hands it over in the order it goes on the air, which is the reverse of the
/// order it is written in.
[[nodiscard]] constexpr std::optional<std::size_t> BytesToHexReversed(std::span<const std::uint8_t> bytes,
                                                                      std::span<char> out) noexcept
{
    if (HexCharCount(bytes.size()) > out.size())
    {
        return std::nullopt;
    }

    std::size_t written = 0U;
    for (std::size_t i = bytes.size(); i > 0U; --i)
    {
        const auto byte = bytes[i - 1U];
        out[written++]  = detail::NibbleToHex(static_cast<std::uint8_t>(byte >> 4U));
        out[written++]  = detail::NibbleToHex(byte);
    }
    return written;
}

} // namespace hwlib::utilities
