
// Copyright Henrik S. Gaßmann 2023.
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/detail/tls.hpp"

#include <dplx/dlog/definitions.hpp>
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

} // namespace dplx::dlog::detail
