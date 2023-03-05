
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <dplx/dlog/arguments.hpp>
#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/fwd.hpp>

namespace dplx::dlog
{

class [[nodiscard]] span_scope
{
    span_context mId{};
    bus_handle *mBus{};

public:
    // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
    severity threshold{severity::none};

    ~span_scope() noexcept
    {
        if (mId.spanId != span_id{} && mBus != nullptr)
        {
            (void)send_close_msg();
        }
    }
    span_scope() noexcept = default;

private:
    span_scope(span_context sctx,
               bus_handle &targetBus,
               severity thresholdInit = default_threshold) noexcept
        : mId(sctx)
        , mBus(&targetBus)
        , threshold(thresholdInit)
    {
    }

    auto send_close_msg() noexcept -> result<void>;

public:
    static auto open(bus_handle &targetBus,
                     std::string_view name,
                     detail::function_location fn) noexcept -> span_scope;
    static auto open(span_scope const &parent,
                     std::string_view name,
                     detail::function_location fn) noexcept -> span_scope;

    [[nodiscard]] auto context() const noexcept -> span_context
    {
        return mId;
    }
    [[nodiscard]] auto bus() const noexcept -> bus_handle *
    {
        return mBus;
    }
};

} // namespace dplx::dlog
