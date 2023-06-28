
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
// type arguments can't be enclosed in parentheses
// NOLINTBEGIN(bugprone-macro-parentheses)

#define DPLX_DLOG_DEFINE_AUTO_TUPLE_CODEC(_fq_type)                            \
    auto ::dplx::dp::codec<_fq_type>::size_of(emit_context &ctx,               \
                                              _fq_type const &value) noexcept  \
            -> std::uint64_t                                                   \
    {                                                                          \
        static_assert(packable_tuple<_fq_type>);                               \
        return dp::size_of_tuple(ctx, value);                                  \
    }                                                                          \
    auto ::dplx::dp::codec<_fq_type>::encode(                                  \
            emit_context &ctx, _fq_type const &value) noexcept -> result<void> \
    {                                                                          \
        return dp::encode_tuple(ctx, value);                                   \
    }                                                                          \
    auto ::dplx::dp::codec<_fq_type>::decode(                                  \
            parse_context &ctx, _fq_type &outValue) noexcept -> result<void>   \
    {                                                                          \
        return dp::decode_tuple(ctx, outValue);                                \
    }

#define DPLX_DLOG_DEFINE_AUTO_OBJECT_CODEC(_fq_type)                           \
    auto ::dplx::dp::codec<_fq_type>::size_of(emit_context &ctx,               \
                                              _fq_type const &value) noexcept  \
            -> std::uint64_t                                                   \
    {                                                                          \
        static_assert(packable_object<_fq_type>);                              \
        return dp::size_of_object(ctx, value);                                 \
    }                                                                          \
    auto ::dplx::dp::codec<_fq_type>::encode(                                  \
            emit_context &ctx, _fq_type const &value) noexcept -> result<void> \
    {                                                                          \
        return dp::encode_object(ctx, value);                                  \
    }                                                                          \
    auto ::dplx::dp::codec<_fq_type>::decode(                                  \
            parse_context &ctx, _fq_type &outValue) noexcept -> result<void>   \
    {                                                                          \
        return dp::decode_object(ctx, outValue);                               \
    }

// NOLINTEND(bugprone-macro-parentheses)
// NOLINTEND(cppcoreguidelines-macro-usage)
