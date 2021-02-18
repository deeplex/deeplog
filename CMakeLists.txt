#[=====================================================================[

Written in 2017, 2019-2021 by Henrik Steffen Gaßmann <henrik@gassmann.onl>

To the extent possible under law, the author(s) have dedicated all
copyright and related and neighboring rights to this software to the
public domain worldwide. This software is distributed without any warranty.

You should have received a copy of the CC0 Public Domain Dedication
along with this software. If not, see

    http://creativecommons.org/publicdomain/zero/1.0/

#]=====================================================================]
cmake_minimum_required(VERSION 3.16)
project(deeplog VERSION 0.0.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/tools/cmake/")
include(EnforceOutOfSourceBuilds)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
    OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
        AND NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC"))
    set(GXX_FRONTEND 1)
endif()

if (MSVC
    OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
        AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC"))
    set(CL_FRONTEND 1)
endif()

########################################################################
# options
include("${CMAKE_SOURCE_DIR}/options.cmake")

if (BUILD_TESTING)
    enable_testing()
endif()

########################################################################
# dependencies

find_package(fmt CONFIG REQUIRED)

find_package(outcome CONFIG REQUIRED)

if (BUILD_TESTING)
    set(DLOG_REQUIRE_UNIT_TEST_FRAMEWORK unit_test_framework)
endif()
find_package(Boost 1.73 REQUIRED COMPONENTS
    ${DLOG_REQUIRE_UNIT_TEST_FRAMEWORK}
)

########################################################################
# warning configuration
include(CompilerWarnings)

########################################################################
# targets
add_library(compiler_settings INTERFACE)
add_library(Deeplex::deeplog_compiler_settings ALIAS compiler_settings)

if (CMAKE_CXX_EXTENSIONS)
    set(CMAKE_CXX_EXTENSIONS OFF CACHE STRING "" FORCE)
endif()
target_compile_features(compiler_settings INTERFACE cxx_std_20)
if (MSVC)
    target_compile_options(compiler_settings INTERFACE
        /utf-8 # all sources are utf-8 encoded
        /permissive- # TPL, disables many MSVC leniencies
        /Zc:__cplusplus # correctly define the __cplusplus macro
    )
endif()

# deeplog library
add_library(deeplog)
add_library(Deeplex::deeplog ALIAS deeplog)
target_include_directories(deeplog PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(deeplog PUBLIC
    Deeplex::deeplog_compiler_settings
    
    fmt::fmt
    outcome::outcome
)

target_sources(deeplog PRIVATE
    "include/dplx/dlog.hpp"

    "include/dplx/dlog/disappointment.hpp"
    "src/dlog/disappointment.cpp"
)

########################################################################
# test target
if (BUILD_TESTING)
    add_executable(deeplog-tests)
    target_link_libraries(deeplog-tests PRIVATE
        Deeplex::deeplog

        Boost::unit_test_framework
        fmt::fmt
    )

    target_sources(deeplog-tests PRIVATE
        "tests/deeplog-tests.main.cpp"
        "tests/boost-test.hpp"
    )

    add_test(NAME deeplog-tests COMMAND deeplog-tests)
endif()

add_subdirectory(apps/deeplog-cli)
