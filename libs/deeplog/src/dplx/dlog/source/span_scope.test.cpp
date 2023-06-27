
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/source/span_scope.hpp"

#include <catch2/catch_test_macros.hpp>

#include <dplx/dlog/bus/buffer_bus.hpp>
#include <dplx/dlog/bus/mpsc_bus.hpp>
#include <dplx/dlog/log_fabric.hpp>
#include <dplx/dlog/macros.hpp>
#include <dplx/dlog/source/log_record_port.hpp>

#include "test_dir.hpp"
#include "test_utils.hpp"

namespace dlog_tests
{

static_assert(!std::is_move_constructible_v<dlog::span_scope>);
static_assert(!std::is_copy_constructible_v<dlog::span_scope>);

TEST_CASE("a span scope can attach to a context")
{
    dlog::log_fabric core{
            dlog::bufferbus(test_dir, TEST_FILE_BB, small_buffer_bus_size)
                    .value()};
    dlog::log_context ctx(core);

    SECTION("to disable a scope")
    {
        ctx.span({{{1}}, {{1}}});
        CHECK(ctx.span().spanId != dlog::span_id::invalid());
        {
            auto noneSpan = dlog::span_scope::none(ctx);
            CHECK(ctx.span().spanId == dlog::span_id::invalid());
            CHECK(ctx.span() == noneSpan.context());
        }
        CHECK(ctx.span().spanId != dlog::span_id::invalid());
    }
    SECTION("to open a span with an implicit parent")
    {
        auto span = dlog::span_scope::open(ctx, "test-span");
        CHECK(ctx.span().spanId != dlog::span_id::invalid());
        CHECK(ctx.span() == span.context());
    }
    SECTION("to open a span with an explicit parent")
    {
        auto span = dlog::span_scope::open(ctx, "test-span",
                                           dlog::span_context{});
        CHECK(ctx.span().spanId != dlog::span_id::invalid());
        CHECK(ctx.span() == span.context());
    }
    SECTION("and shadow its implicit parent")
    {
        auto outerSpan = dlog::span_scope::open(ctx, "outer");
        CHECK(ctx.span() == outerSpan.context());

        {
            auto innerSpan = dlog::span_scope::open(ctx, "inner");
            CHECK(ctx.span() == innerSpan.context());
        }
        CHECK(ctx.span() == outerSpan.context());
    }
#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
    SECTION("to open a span with an implicit context")
    {
        dlog::set_thread_context(ctx);
        auto span = dlog::span_scope::open("implicit-test-span");
        CHECK(dlog::detail::active_context().span() == span.context());
    }
    SECTION("to open a span with an implicit context and an explicit parent")
    {
        dlog::set_thread_context(ctx);
        auto span = dlog::span_scope::open("implicit-test-span",
                                           dlog::span_context{});
        CHECK(dlog::detail::active_context().span() == span.context());
    }
#endif
}

TEST_CASE("the correct explicit span scope is attached to log records")
{
    dlog::log_fabric core{
            dlog::bufferbus(test_dir, TEST_FILE_BB, small_buffer_bus_size)
                    .value()};
    dlog::log_context ctx(core);

    {
        auto outerSpan = dlog::span_scope::open(ctx, "sample");
        DLOG_TO(ctx, dlog::severity::warn,
                "this should be attached to the root span");

        {
            auto innerSpan = dlog::span_scope::open(ctx, "inner");
            DLOG_TO(ctx, dlog::severity::warn,
                    "this should be attached to the inner span.");
        }

        DLOG_TO(ctx, dlog::severity::warn,
                "this should be attached to the root span, too");
    }
}

#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
TEST_CASE("the correct implicit span scope is attached to log records")
{
    dlog::log_fabric core{
            dlog::bufferbus(test_dir, TEST_FILE_BB, small_buffer_bus_size)
                    .value()};
    dlog::set_thread_context(dlog::log_context(core));

    {
        auto log = dlog::span_scope::open("outer");
        DLOG_(warn, "this should be attached to the root span");

        {
            auto innerLog = dlog::span_scope::open("inner");
            DLOG_(warn, "this should be attached to the inner span.");
        }
        DLOG_(warn, "this should be attached to the root span, too");
    }
}
#endif

} // namespace dlog_tests
