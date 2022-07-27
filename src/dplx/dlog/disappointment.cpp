
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include <dplx/dlog/disappointment.hpp>

#include <fmt/format.h>

namespace dplx::dlog
{

class error_category_impl : public std::error_category
{
public:
    virtual auto name() const noexcept -> const char * override;
    virtual auto message(int errval) const -> std::string override;
};

auto error_category_impl::name() const noexcept -> const char *
{
    return "dplx::dlog error category";
}

auto error_category_impl::message(int code) const -> std::string
{
    using namespace std::string_literals;
    switch (static_cast<errc>(code))
    {
    case errc::nothing:
        return "no error/success"s;
    case errc::bad:
        return "an external API did not meet its operation contract"s;

    default:
        return fmt::format(FMT_STRING("unknown code {}"), code);
    }
}

error_category_impl const error_category_instance{};

auto error_category() noexcept -> std::error_category const &
{
    return error_category_instance;
}

} // namespace dplx::dlog
