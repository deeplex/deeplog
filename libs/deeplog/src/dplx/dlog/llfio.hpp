
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <dplx/predef/compiler/visualc.h>
#include <dplx/predef/os/windows.h>

#if defined(DPLX_COMP_MSVC_AVAILABLE)
#pragma warning(push, 2)
#pragma warning(disable : 4906 4905)
#endif

#include <llfio/llfio.hpp>

#if defined(DPLX_COMP_MSVC_AVAILABLE)
#pragma warning(pop)
#endif

namespace dplx::dlog
{

namespace llfio = LLFIO_V2_NAMESPACE;

namespace detail
{

// wraps a llfio byte io read operation and normalizes EOF handling to POSIX
inline auto
xread(llfio::byte_io_handle &h,
      llfio::byte_io_handle::io_request<llfio::byte_io_handle::buffers_type>
              reqs,
      llfio::deadline d = llfio::deadline()) noexcept
        -> llfio::byte_io_handle::io_result<llfio::byte_io_handle::buffers_type>
{
    using buffers_type = llfio::byte_io_handle::buffers_type;
    using io_result = llfio::byte_io_handle::io_result<buffers_type>;
    io_result readRx = h.read(reqs, d);

#if defined(DPLX_OS_WINDOWS_AVAILABLE)

    if (readRx.has_error()) [[unlikely]]
    {
        constexpr system_error2::win32_code error_handle_eof(38U);
        constexpr system_error2::nt_code status_end_of_file(
                static_cast<system_error2::nt_code::value_type>(0xC0000011));

        if (auto const &ec = readRx.assume_error();
            (ec.domain() == status_end_of_file.domain()
             && ec.value() == status_end_of_file.value())
            || (ec.domain() == error_handle_eof.domain()
                && ec.value() == error_handle_eof.value()))
        {
            for (auto &buffer : reqs.buffers)
            {
                buffer = {buffer.data(), 0U};
            }
            readRx = buffers_type(reqs.buffers.data(),
                                  reqs.buffers.empty() ? 0U : 1U);
        }
    }
#endif
    return readRx;
}

} // namespace detail

} // namespace dplx::dlog
