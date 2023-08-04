
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/record_container.hpp"

#include <dplx/dp.hpp>
#include <dplx/dp/codecs/auto_object.hpp>
#include <dplx/dp/codecs/core.hpp>

#include <dplx/dlog/detail/utils.hpp>

DPLX_DLOG_DEFINE_AUTO_OBJECT_CODEC(::dplx::dlog::record_resource)
