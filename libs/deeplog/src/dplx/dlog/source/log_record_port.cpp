
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/source/log_record_port.hpp"

namespace dplx::dlog
{

namespace
{

// derivative of xxHASH64
// https://github.com/Cyan4973/xxHash/blob/2b328a10983d232364ceda15df1d33531b5f0eb5/doc/xxhash_spec.md
constexpr std::uint64_t PRIME64_1 = 0x9e37'79b1'85eb'ca87U;
constexpr std::uint64_t PRIME64_2 = 0xc2b2'ae3d'27d4'eb4fU;
constexpr std::uint64_t PRIME64_3 = 0x1656'67b1'9e37'79f9U;
constexpr std::uint64_t PRIME64_4 = 0x85eb'ca77'c2b2'ae63U;
constexpr std::uint64_t PRIME64_5 = 0x27d4'eb2f'1656'67c5U;

DPLX_ATTR_FORCE_INLINE constexpr auto
xxHash64_round(std::uint64_t accN, std::uint64_t const laneN) noexcept
        -> std::uint64_t
{
    accN += laneN * PRIME64_2;
    accN <<= 31; // NOLINT(cppcoreguidelines-avoid-magic-numbers)
    return accN * PRIME64_1;
}

} // namespace

namespace detail
{

auto derive_span_id(std::uint64_t traceIdP0,
                    std::uint64_t traceIdP1,
                    std::uint64_t ctr) noexcept -> span_id
{
    // initialize
    std::uint64_t acc = PRIME64_5;
    // add input length
    acc += 3 * sizeof(std::uint64_t);

    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)

    // "remaining" input
    acc ^= xxHash64_round(0U, traceIdP0);
    acc = (acc << 27) * PRIME64_1;
    acc += PRIME64_4;
    acc ^= xxHash64_round(0U, traceIdP1);
    acc = (acc << 27) * PRIME64_1;
    acc += PRIME64_4;
    acc ^= xxHash64_round(0U, ctr);
    acc = (acc << 27) * PRIME64_1;
    acc += PRIME64_4;

    // mix
    acc ^= acc >> 33;
    acc *= PRIME64_2;
    acc ^= acc >> 29;
    acc *= PRIME64_3;
    acc ^= acc >> 32;

    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

    return std::bit_cast<span_id>(acc);
}

} // namespace detail

} // namespace dplx::dlog
