
// Copyright Henrik S. Gaßmann 2023.
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/source.hpp"

#include <catch2/catch_test_macros.hpp>

#include <dplx/dp/codecs/core.hpp>
#include <dplx/dp/codecs/std-string.hpp>
#include <dplx/dp/items/parse_core.hpp>

#include <dplx/dlog/bus/mpsc_bus.hpp>
#include <dplx/dlog/core.hpp>
#include <dplx/dlog/macros.hpp>

#include "test_dir.hpp"
#include "test_utils.hpp"

namespace dlog_tests
{

namespace
{

class custom_loggable
{
};

} // namespace

} // namespace dlog_tests

template <>
struct fmt::formatter<dlog_tests::custom_loggable>
{
    static constexpr auto parse(format_parse_context &ctx)
            -> decltype(ctx.begin())
    {
        auto const *it = ctx.begin();
        if (it == ctx.end() || *it != '}')
        {
            throw format_error("invalid format");
        }
        return it;
    }
    template <typename FormatContext>
    static auto format(dlog_tests::custom_loggable, FormatContext &ctx)
            -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "[custom]");
    }
};
template <>
struct dplx::dp::codec<dlog_tests::custom_loggable>
{
    static constexpr auto size_of(emit_context &,
                                  dlog_tests::custom_loggable) noexcept
            -> std::uint64_t
    {
        return 1U;
    }
    static auto encode(emit_context &ctx, dlog_tests::custom_loggable) noexcept
            -> result<void>
    {
        return dp::encode(ctx, 0U);
    }
    static auto decode(parse_context &ctx,
                       dlog_tests::custom_loggable &) noexcept -> result<void>
    {
        return dp::expect_item_head(ctx, type_code::posint, 0U);
    }
};
template <>
struct dplx::dlog::reification_tag<dlog_tests::custom_loggable>
    : user_reification_type_constant<
              1000U> // NOLINT(cppcoreguidelines-avoid-magic-numbers)
{
};

namespace dlog_tests
{

TEST_CASE("The logger can write a message")
{
    constexpr auto regionSize = 1 << 14;
    dlog::core core{
            dlog::mpsc_bus(test_dir, "t1.dmsb", 4U, regionSize).value()};

    DLOG_GENERIC_EX(core.connector(), dlog::severity::warn,
                    "important msg without arg");

    auto retireRx = core.retire_log_records();
    REQUIRE(retireRx);
    CHECK(retireRx.assume_value() == 0);
}
TEST_CASE("The logger can write a message with an int and a string")
{
    constexpr auto regionSize = 1 << 14;
    dlog::core core{
            dlog::mpsc_bus(test_dir, "t2.dmsb", 4U, regionSize).value()};

    DLOG_GENERIC_EX(core.connector(), dlog::severity::warn,
                    "important msg with arg {} and {}", 1, "it's me Mario");

    auto retireRx = core.retire_log_records();
    REQUIRE(retireRx);
    CHECK(retireRx.assume_value() == 0);
}
TEST_CASE("The logger can write a message with a custom type")
{
    constexpr auto regionSize = 1 << 14;
    dlog::core core{
            dlog::mpsc_bus(test_dir, "t3.dmsb", 4U, regionSize).value()};

    DLOG_GENERIC_EX(core.connector(), dlog::severity::warn,
                    "important msg with arg {}", custom_loggable{});

    auto retireRx = core.retire_log_records();
    REQUIRE(retireRx);
    CHECK(retireRx.assume_value() == 0);
}

} // namespace dlog_tests
