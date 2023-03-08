
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <dplx/dlog/config.hpp>
#include <dplx/dlog/fwd.hpp>

namespace dplx::dlog::detail
{

#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern thread_local constinit span_scope const *active_span;

void deactivate_span() noexcept;
#endif

} // namespace dplx::dlog::detail
