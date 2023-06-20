# Written in 2023 by Henrik Steffen Gaßmann <henrik@gassmann.onl>
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
# library project
add_library(deeplog)
add_library(Deeplex::deeplog ALIAS deeplog)

target_link_libraries(deeplog PUBLIC Deeplex::deeplog_compiler_settings)
target_compile_definitions(deeplog
    PRIVATE DPLX_DLOG_BUILDING
)
if (BUILD_SHARED_LIBS)
    target_compile_definitions(deeplog 
        PUBLIC DPLX_DLOG_DYNAMIC
        PRIVATE DPLX_DLOG_DYNAMIC_EXPORTS
    )
endif()

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
# sources
dplx_target_sources(deeplog
    TEST_TARGET deeplog-tests
    MODE SMART_SOURCE MERGED_LAYOUT
    BASE_DIR dplx

    PUBLIC
        dlog

        dlog/core/log_clock
        dlog/core/strong_types

        dlog/source/log
        dlog/source/log_context
        dlog/source/log_record_port
        dlog/source/span_scope

        dlog/definitions

        dlog/attributes
        dlog/attribute_transmorpher
        dlog/core
        dlog/log_bus
        dlog/sink

        dlog/bus/buffer_bus
        dlog/bus/mpsc_bus

        dlog/record_container

        dlog/file_database

        dlog/detail/interleaving_stream
        dlog/detail/tls
)

dplx_target_sources(deeplog
    TEST_TARGET deeplog-tests
    MODE SMART_HEADER_ONLY MERGED_LAYOUT
    BASE_DIR dplx

    PUBLIC
        dlog/any_attribute
        dlog/config
        dlog/disappointment
        dlog/fwd
        dlog/loggable

        dlog/detail/any_loggable_ref
        dlog/detail/any_reified
        dlog/detail/workaround
)

dplx_target_sources(deeplog
    MODE VERBATIM MERGED_LAYOUT
    BASE_DIR dplx

    PUBLIC
        dlog/concepts.hpp

        dlog/arguments.hpp
        dlog/argument_transmorpher_fmt.hpp
        dlog/attributes.hpp
        dlog/macros.hpp

        dlog/detail/codec_dummy.hpp
        dlog/detail/hex.hpp
        dlog/detail/utils.hpp
        dlog/detail/x_poly_types.inl

        dlog/detail/file_stream.hpp

        dlog/source/record_output_buffer.hpp
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated/src/dplx/dlog/detail)
configure_file(../../tools/config.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/generated/src/dplx/dlog/detail/config.hpp @ONLY)
target_sources(deeplog PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated/src/dplx/dlog/detail/config.hpp>)

if (BUILD_TESTING)
    dplx_target_sources(deeplog-tests PRIVATE
        MODE VERBATIM
        BASE_DIR dlog_tests

        PRIVATE
            test_dir.hpp
            test_utils.hpp
    )
endif ()

########################################################################
# install script
install(DIRECTORY src/dplx
    TYPE INCLUDE
    FILES_MATCHING
    REGEX ".*\\.(h(pp)?|inl)"
)
