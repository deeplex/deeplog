
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "ecomponent.hpp"

#include <dplx/dlog.hpp>

namespace dlog_ex
{

class jsf_engine
{
    std::uint32_t a;
    std::uint32_t b;
    std::uint32_t c;
    std::uint32_t d;

public:
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
    jsf_engine(std::uint32_t seed) noexcept
        : a(0xf1ea'5eedU)
        , b(seed)
        , c(seed)
        , d(seed)
    {
        for (int i = 0; i < 20; ++i)
        {
            (void)operator()();
        }
    }
    auto operator()() noexcept -> std::uint32_t
    {
        std::uint32_t e = a - std::rotl(b, 27);
        a = b ^ std::rotl(c, 17);
        b = c + d;
        c = d + e;
        d = e + a;
        return d;
    }
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
};

void do_output()
{
    auto fnScope = dplx::dlog::span_scope::open("do_output");

    DLOG_(warn, "important msg with arg {}", 1);
    DLOG_(info, "here happens something else");
    DLOG_(error, "oh no something bad happened");

    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
    jsf_engine gen(0xdead'beefU);
    constexpr int limit = 0x2a * 2;
    for (int i = 0; i < limit; ++i)
    {
        if (auto v = gen(); (v & 0x800'0000U) != 0)
        {
            DLOG_(warn, "{} is a pretty big number", v);
        }
        if (auto v = gen(); (v & 1U) != 0)
        {
            DLOG_(info, "{} is a real oddity", v);
        }
        DLOG_(debug, "I'm still alive");
        if (auto v = gen(); (v & 0x7) == 0x7)
        {
            DLOG_(error, "this is not good");
        }
    }
    DLOG_(fatal, "this is the end");
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
}

} // namespace dlog_ex
