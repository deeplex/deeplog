
// Copyright Henrik Steffen Gaßmann 2021,2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/detail/platform.hpp"

#include <bit>

#include <dplx/predef/os.h>

#if defined(DPLX_OS_WINDOWS_AVAILABLE)
#include <boost/winapi/get_current_process_id.hpp>
#elif defined(DPLX_OS_UNIX_AVAILABLE) || defined(DPLX_OS_MACOS_AVAILABLE)
#include <unistd.h>
#endif

namespace dplx::dlog::detail
{

#if defined(DPLX_OS_WINDOWS_AVAILABLE)
auto get_current_process_id() noexcept -> std::uint32_t
{
    return std::bit_cast<std::uint32_t>(boost::winapi::GetCurrentProcessId());
}
#elif defined(DPLX_OS_UNIX_AVAILABLE) || defined(DPLX_OS_MACOS_AVAILABLE)
auto get_current_process_id() noexcept -> std::uint32_t
{
    return std::bit_cast<std::uint32_t>(::getpid());
}
#else
auto get_current_process_id() noexcept -> std::uint32_t
{
    return 0xffff'ffff; // NOLINT(cppcoreguidelines-avoid-magic-numbers)
}
#endif

} // namespace dplx::dlog::detail
