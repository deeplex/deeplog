
// Copyright Henrik Steffen Gaßmann 2021, 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <dplx/dp.hpp>

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

template <>
class dplx::dp::codec<dplx::dlog::detail::decodable_dummy>
{
public:
    static auto size_of(emit_context const &,
                        dplx::dlog::detail::decodable_dummy) noexcept
            -> std::uint64_t;
    static auto encode(emit_context const &,
                       dplx::dlog::detail::decodable_dummy) noexcept
            -> result<void>;
    static auto decode(parse_context &,
                       dplx::dlog::detail::decodable_dummy &) noexcept
            -> result<void>;
};
template <>
class dplx::dp::codec<dplx::dlog::detail::encodable_dummy>
{
public:
    static auto size_of(emit_context const &,
                        dplx::dlog::detail::encodable_dummy) noexcept
            -> std::uint64_t;
    static auto encode(emit_context const &,
                       dplx::dlog::detail::encodable_dummy) noexcept
            -> result<void>;
    static auto decode(parse_context &,
                       dplx::dlog::detail::encodable_dummy &) noexcept
            -> result<void>;
};
template <>
class dplx::dp::codec<dplx::dlog::detail::codec_dummy>
{
public:
    static auto size_of(emit_context const &,
                        dplx::dlog::detail::codec_dummy) noexcept
            -> std::uint64_t;
    static auto encode(emit_context const &,
                       dplx::dlog::detail::codec_dummy) noexcept
            -> result<void>;
    static auto decode(parse_context &,
                       dplx::dlog::detail::codec_dummy &) noexcept
            -> result<void>;
};
