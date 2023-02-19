
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>

#include <dplx/dp/disappointment.hpp>
#include <dplx/dp/streams/output_buffer.hpp>

namespace dplx::dlog
{

// the class is final and none of its base classes have public destructors
// NOLINTNEXTLINE(cppcoreguidelines-virtual-class-destructor)
class bus_output_buffer : public dp::output_buffer
{
protected:
    constexpr ~bus_output_buffer() noexcept = default;
    using output_buffer::output_buffer;

private:
    auto do_grow([[maybe_unused]] size_type requestedSize) noexcept
            -> dp::result<void> final
    {
        return dp::errc::end_of_stream;
    }
    auto do_bulk_write([[maybe_unused]] std::byte const *src,
                       [[maybe_unused]] std::size_t size) noexcept
            -> dp::result<void> final
    {
        return dp::errc::end_of_stream;
    }
};

} // namespace dplx::dlog
