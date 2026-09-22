#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <integra/hex_string.hpp>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace
{

using integra::BytesToHex;
using integra::BytesToHexReversed;
using integra::HexCharCount;
using integra::HexToBytes;

[[nodiscard]] std::string Formatted(std::span<const std::uint8_t> bytes)
{
    std::array<char, 64> out{};
    const auto written = BytesToHex(bytes, out);
    EXPECT_TRUE(written.has_value());
    return std::string{out.data(), written.value_or(0U)};
}

TEST(HexStringTest, FormatsBytesAsLowerCaseHex)
{
    constexpr std::array<std::uint8_t, 4> BYTES{0x00U, 0x0FU, 0xA5U, 0xFFU};
    EXPECT_EQ(Formatted(BYTES), "000fa5ff");
}

TEST(HexStringTest, KeepsTheLeadingZeroOfASmallByte)
{
    constexpr std::array<std::uint8_t, 1> BYTES{0x07U};
    EXPECT_EQ(Formatted(BYTES), "07");
}

TEST(HexStringTest, FormatsAMacLeastSignificantByteFirst)
{
    // As a radio hands it over, against how it is written down.
    constexpr std::array<std::uint8_t, 6> MAC{0x56U, 0x34U, 0x12U, 0xF0U, 0xDEU, 0xBCU};

    std::array<char, HexCharCount(MAC.size()) + 1U> out{};
    const auto written = BytesToHexReversed(MAC, out);

    ASSERT_TRUE(written.has_value());
    EXPECT_EQ(written.value(), 12U);
    EXPECT_EQ(std::string_view{out.data()}, "bcdef0123456");
    // Value-initialised, so what was written is also a C string.
    EXPECT_EQ(out.back(), '\0');
}

TEST(HexStringTest, ParsesHexIntoBytes)
{
    std::array<std::uint8_t, 4> out{};
    const auto count = HexToBytes("000fa5ff", out);

    ASSERT_TRUE(count.has_value());
    EXPECT_EQ(count.value(), 4U);
    EXPECT_EQ(out, (std::array<std::uint8_t, 4>{0x00U, 0x0FU, 0xA5U, 0xFFU}));
}

TEST(HexStringTest, ParsesUpperCaseAndMixedCase)
{
    std::array<std::uint8_t, 2> out{};
    ASSERT_TRUE(HexToBytes("AbCd", out).has_value());
    EXPECT_EQ(out, (std::array<std::uint8_t, 2>{0xABU, 0xCDU}));
}

TEST(HexStringTest, RoundTrips)
{
    constexpr std::string_view INPUT = "0123456789abcdef";
    std::array<std::uint8_t, 8> bytes{};

    const auto count = HexToBytes(INPUT, bytes);
    ASSERT_TRUE(count.has_value());
    EXPECT_EQ(Formatted(std::span{bytes}.first(count.value())), INPUT);
}

TEST(HexStringTest, RejectsAnOddLength)
{
    std::array<std::uint8_t, 4> out{0xEEU, 0xEEU, 0xEEU, 0xEEU};
    EXPECT_FALSE(HexToBytes("abc", out).has_value());
    // Nothing written, so a caller that ignored the result cannot act on half a value.
    EXPECT_EQ(out.front(), 0xEEU);
}

TEST(HexStringTest, RejectsANonHexCharacter)
{
    std::array<std::uint8_t, 4> out{};
    EXPECT_FALSE(HexToBytes("00gg", out).has_value());
}

TEST(HexStringTest, RejectsAPartiallyParsableByte)
{
    // std::from_chars reports success here, having consumed only the '0', which is
    // how one of the two originals turned "0x" into a zero byte.
    std::array<std::uint8_t, 4> out{};
    EXPECT_FALSE(HexToBytes("0x", out).has_value());
    EXPECT_FALSE(HexToBytes("0x41", out).has_value());
}

TEST(HexStringTest, RejectsANegativeChar)
{
    // std::isxdigit is undefined for a negative char; the character below is one on
    // a platform where char is signed, and it has to be rejected, not crash.
    const std::string input{static_cast<char>(0x80), static_cast<char>(0x81)};
    std::array<std::uint8_t, 4> out{};
    EXPECT_FALSE(HexToBytes(input, out).has_value());
}

TEST(HexStringTest, RefusesToOverflowTheOutput)
{
    std::array<std::uint8_t, 1> small{};
    EXPECT_FALSE(HexToBytes("0011", small).has_value());

    constexpr std::array<std::uint8_t, 2> BYTES{0x00U, 0x11U};
    std::array<char, 3> tooSmall{};
    EXPECT_FALSE(BytesToHex(BYTES, tooSmall).has_value());
    EXPECT_FALSE(BytesToHexReversed(BYTES, tooSmall).has_value());
    // Not one character of a half-written value.
    EXPECT_EQ(tooSmall.front(), '\0');
}

TEST(HexStringTest, AcceptsAnExactlySizedOutput)
{
    constexpr std::array<std::uint8_t, 2> BYTES{0x00U, 0x11U};
    std::array<char, 4> exact{};
    const auto written = BytesToHex(BYTES, exact);

    ASSERT_TRUE(written.has_value());
    EXPECT_EQ(written.value(), HexCharCount(BYTES.size()));
}

TEST(HexStringTest, HandlesEmptyInput)
{
    std::array<std::uint8_t, 4> bytes{};
    const auto count = HexToBytes("", bytes);
    ASSERT_TRUE(count.has_value());
    EXPECT_EQ(count.value(), 0U);

    std::array<char, 4> out{};
    const auto written = BytesToHex({}, out);
    ASSERT_TRUE(written.has_value());
    EXPECT_EQ(written.value(), 0U);
}

TEST(HexStringTest, RunsAtCompileTime)
{
    // Both directions are constexpr, so a fixed value costs nothing at run time.
    static constexpr auto PARSED = [] {
        std::array<std::uint8_t, 2> bytes{};
        const auto count = HexToBytes("beef", bytes);
        return std::pair{bytes, count.value_or(0U)};
    }();
    static_assert(PARSED.second == 2U);
    static_assert(PARSED.first[0] == 0xBEU);
    static_assert(PARSED.first[1] == 0xEFU);

    static constexpr auto FORMATTED = [] {
        std::array<char, 5> out{};
        const std::array<std::uint8_t, 2> bytes{0xBEU, 0xEFU};
        std::ignore = BytesToHex(bytes, out);
        return out;
    }();
    static_assert(FORMATTED[0] == 'b');
    static_assert(FORMATTED[3] == 'f');

    SUCCEED();
}

} // namespace
