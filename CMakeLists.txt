# Written in 2017, 2019 by Henrik Steffen Gaßmann <henrik@gassmann.onl>
#
# To the extent possible under law, the author(s) have dedicated all
# copyright and related and neighboring rights to this software to the
# public domain worldwide. This software is distributed without any warranty.
#
# You should have received a copy of the CC0 Public Domain Dedication
# along with this software. If not, see
#
#     http://creativecommons.org/publicdomain/zero/1.0/
#
########################################################################
cmake_minimum_required(VERSION 3.20)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/tools/cmake/")

########################################################################
# configure vcpkg from environment vars if possible
include(VcpkgDefaults)

if (BUILD_TESTING OR NOT DEFINED BUILD_TESTING)
    list(APPEND VCPKG_MANIFEST_FEATURES "tests")
endif()

########################################################################
project(deeplog
    VERSION 0.0.0.0
    LANGUAGES CXX
)

include(EnforceOutOfSourceBuilds)
include(CMakeDependentOption)
include(CMakePackageConfigHelpers)
include(FeatureSummary)
include(GNUInstallDirs)

include(SourceHelpers)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
if (BUILD_TESTING)
    enable_testing()
endif()


set(DLOG_LLFIO_TARGET "sl" CACHE STRING "The llfio target to link against, can be 'hl', 'sl' or 'dl'")
set_property(CACHE DLOG_LLFIO_TARGET PROPERTY STRINGS "hl;sl;dl")


########################################################################
# options
option(DPLX_DLOG_DISABLE_WORKAROUNDS "Disable all workarounds" OFF)
option(DPLX_DLOG_FLAG_OUTDATED_WORKAROUNDS "Emit compiler errors for workarounds which are active, but haven't been validated for this version" OFF)

option(DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT "Disable implicit context support via TLS" OFF)

option(BUILD_EXAMPLES "Build the example executables" OFF)

########################################################################
# dependencies

set(Boost_NO_WARN_NEW_VERSIONS ON)

find_package(fmt CONFIG REQUIRED)

find_package(ftxui CONFIG REQUIRED)

find_package(outcome CONFIG REQUIRED)

find_package(llfio CONFIG REQUIRED)

find_package(concrete CONFIG REQUIRED)
find_package(deeppack CONFIG REQUIRED)

find_package(Boost 1.81 REQUIRED)

find_package(Catch2 CONFIG)
set_package_properties(Catch2 PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Allows to build the test suite"
)
cmake_dependent_option(BUILD_TESTING "Build the documentation using sphinx" ON Catch2_FOUND OFF)

find_package(Sphinx)
set_package_properties(Sphinx PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Allows to build the documentation"
)
cmake_dependent_option(BUILD_DOCS "Build the documentation using sphinx" ON Sphinx_FOUND OFF)


########################################################################
# warning configuration
include(CompilerWarnings)


########################################################################
# additional compiler options
set(CMAKE_CXX_EXTENSIONS OFF CACHE STRING "" FORCE)

add_library(compiler_settings INTERFACE)
add_library(Deeplex::deeplog_compiler_settings ALIAS compiler_settings)
set_target_properties(compiler_settings PROPERTIES
    EXPORT_NAME deeplog_compiler_settings
)

target_compile_features(compiler_settings INTERFACE cxx_std_20)
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(compiler_settings INTERFACE
        /utf-8 # all sources are utf-8 encoded
        /Zc:__cplusplus # correctly define the __cplusplus macro
    )
endif()


########################################################################
# library
add_library(deeplog)
add_library(Deeplex::deeplog ALIAS deeplog)

target_link_libraries(deeplog PUBLIC Deeplex::deeplog_compiler_settings)

target_include_directories(deeplog PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated/src>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(deeplog PUBLIC
    Deeplex::deeppack

    fmt::fmt
    llfio::${DLOG_LLFIO_TARGET}
    outcome::hl
)

########################################################################
# library test project
if (BUILD_TESTING)
    add_executable(deeplog-tests)
    source_group(TREE dlog_tests PREFIX test_utils)

    target_link_libraries(deeplog-tests PRIVATE
        Deeplex::deeplog

        Catch2::Catch2 Catch2::Catch2WithMain
    )

    target_include_directories(deeplog-tests PRIVATE
        src/dlog_tests
    )

    add_test(NAME deeplog-tests COMMAND deeplog-tests)
endif()

########################################################################
# cli projects
add_subdirectory(apps/deeplog-cli)

########################################################################
# source files
include(sources.cmake)
source_group(TREE src/dplx)

########################################################################
# examples
if (BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()


########################################################################
# docs
if (BUILD_DOCS)
    add_subdirectory(docs)
endif()


########################################################################
# cmake install
install(DIRECTORY src/dplx
    TYPE INCLUDE
    FILES_MATCHING
    REGEX ".*\\.(h(pp)?|inl)"
)

install(TARGETS deeplog compiler_settings EXPORT deeplog-targets)
install(EXPORT deeplog-targets
    NAMESPACE Deeplex::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/deeplog"
)

configure_package_config_file(tools/deeplog-config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/deeplog-config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/deeplog"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/deeplog-config-version.cmake"
    COMPATIBILITY SameMinorVersion
)

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/deeplog-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/deeplog-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/deeplog"
)


########################################################################
feature_summary(WHAT PACKAGES_FOUND PACKAGES_NOT_FOUND)
