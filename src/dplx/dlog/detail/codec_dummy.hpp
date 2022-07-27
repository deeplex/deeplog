
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <dplx/dp/decoder/api.hpp>
#include <dplx/dp/encoder/api.hpp>

namespace dplx::dlog::detail
{

struct decodable_dummy
{
};
struct encodable_dummy
{
};
struct codec_dummy
    : encodable_dummy
    , decodable_dummy
{
};

} // namespace dplx::dlog::detail

namespace dplx::dp
{

template <input_stream Stream>
class basic_decoder<dplx::dlog::detail::decodable_dummy, Stream>
{
public:
    using value_type = dplx::dlog::detail::decodable_dummy;

    auto operator()(Stream &stream, value_type &) const noexcept -> result<void>
    {
        return oc::success();
    }
};

template <output_stream Stream>
class basic_encoder<dplx::dlog::detail::encodable_dummy, Stream>
{
public:
    using value_type = dplx::dlog::detail::encodable_dummy;

    auto operator()(Stream &stream, value_type const &) const noexcept
            -> result<void>
    {
        return oc::success();
    }
};

} // namespace dplx::dp
