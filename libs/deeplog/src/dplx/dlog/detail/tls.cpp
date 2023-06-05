
// Copyright Henrik S. Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/detail/tls.hpp"

#include <type_traits>

#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/detail/workaround.hpp>
#include <dplx/dlog/source/log_context.hpp>
#include <dplx/dlog/span_scope.hpp>

#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
thread_local constinit dplx::dlog::span_scope const
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
        *dplx::dlog::detail::active_span{};
#endif

namespace dplx::dlog::detail
{

#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
void deactivate_span() noexcept
{
    active_span = active_span->parent_scope();
}
#endif

#if DPLX_DLOG_WORKAROUND_ISSUE_DEVCOM_1406069

struct most_trivial_string_view
{
    char const *data_;
    std::size_t size_;
};
struct log_context_
{
    log_record_port *mTargetPort;
    span_scope const *scope;
    most_trivial_string_view x;
    severity mThresholdCache;
};

#if __cpp_lib_is_layout_compatible >= 201907L
static_assert(std::is_layout_compatible_v<log_context, log_context_>);
static_assert(std::is_layout_compatible_v<log_context_, log_context>);
#endif

static thread_local constinit log_context_ active_context_{
        nullptr, nullptr, {}, severity{0xffU}};

auto active_context() noexcept -> log_context
{
    return std::bit_cast<log_context>(active_context_);
}
void active_context(log_context nextActiveContext) noexcept
{
    active_context_ = std::bit_cast<log_context_>(nextActiveContext);
}

#else // ^^^ workaround DevCom-1406069 / no workaround vvv

static thread_local constinit log_context active_context_{};

auto active_context() noexcept -> log_context
{
    return active_context_;
}
void active_context(log_context nextActiveContext) noexcept
{
    active_context_ = nextActiveContext;
}

#endif // ^^^ no workaround ^^^

} // namespace dplx::dlog::detail
