
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>

#include <dplx/dp/decoder/api.hpp>
#include <dplx/dp/decoder/core.hpp>
#include <dplx/dp/encoder/api.hpp>
#include <dplx/dp/encoder/core.hpp>
#include <dplx/dp/item_emitter.hpp>
#include <dplx/dp/item_parser.hpp>

// span aliasse
namespace dplx::dlog
{

using bytes = std::span<std::byte const>;
using writable_bytes = std::span<std::byte>;

} // namespace dplx::dlog

// resource id
namespace dplx::dlog
{

enum class resource_id : std::uint64_t
{
};

} // namespace dplx::dlog

// severity
namespace dplx::dlog
{

enum class severity : unsigned
{
    none = 0,
    critical = 1,
    error = 2,
    warn = 3,
    info = 4,
    debug = 5,
    trace = 6,
};

inline constexpr severity default_threshold = severity::warn;

} // namespace dplx::dlog

namespace dplx::dp
{

template <input_stream Stream>
class basic_decoder<dlog::severity, Stream>
{
    using parse = item_parser<Stream>;
    using underlying_type = std::underlying_type_t<dlog::severity>;

public:
    using value_type = dlog::severity;

    auto operator()(Stream &inStream, value_type &value) -> result<void>
    {
        DPLX_TRY(auto parsed,
                 parse::template integer<underlying_type>(
                         inStream, cncr::to_underlying(value_type::trace),
                         parse_mode::canonical));

        value = static_cast<value_type>(parsed);
        return oc::success();
    }
};

template <output_stream Stream>
class basic_encoder<dlog::severity, Stream>
{
    using emit = item_emitter<Stream>;

public:
    using value_type = dlog::severity;

    auto operator()(Stream &outStream, value_type value) -> result<void>
    {
        auto bits = cncr::to_underlying(value);
        if (bits > cncr::to_underlying(value_type::trace))
        {
            return errc::item_value_out_of_range;
        }
        DPLX_TRY(auto &&proxy, write(outStream, 1));
        // single byte posint encoding
        std::ranges::data(proxy)[0] = static_cast<std::byte>(bits);
        if constexpr (lazy_output_stream<Stream>)
        {
            DPLX_TRY(commit(outStream, proxy));
        }
        return oc::success();
    }
};

constexpr auto tag_invoke(encoded_size_of_fn, dlog::severity) -> unsigned
{
    return 1U;
}

} // namespace dplx::dp