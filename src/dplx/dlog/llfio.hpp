
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <boost/predef/compiler.h>

#if defined BOOST_COMP_MSVC_AVAILABLE
#pragma warning(push, 2)
#pragma warning(disable : 4906 4905)
#endif

#include <llfio/llfio.hpp>

#if defined BOOST_COMP_MSVC_AVAILABLE
#pragma warning(pop)
#endif

namespace dplx::dlog
{

namespace llfio = LLFIO_V2_NAMESPACE;

} // namespace dplx::dlog
