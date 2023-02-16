
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <dplx/cncr/utils.hpp>

#include <dplx/dlog/concepts.hpp>

namespace dplx::dlog
{

template <bus Bus>
struct bus_write_lock
{
    using bus_type = Bus;
    using token_type = typename bus_type::logger_token;

    bus_type &bus;
    token_type &token;

    DPLX_ATTR_FORCE_INLINE explicit bus_write_lock(
            bus_type &busRef, token_type &tokenRef) noexcept
        : bus(busRef)
        , token(tokenRef)
    {
    }
    DPLX_ATTR_FORCE_INLINE ~bus_write_lock() noexcept
    {
        bus.commit(token);
    }
};

} // namespace dplx::dlog
