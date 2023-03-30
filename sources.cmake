
dplx_target_sources(deeplog
    TEST_TARGET deeplog-tests
    MODE SMART_SOURCE MERGED_LAYOUT
    BASE_DIR dplx

    PUBLIC
        dlog

        dlog/definitions

        dlog/attributes
        dlog/attribute_transmorpher
        dlog/core
        dlog/log_bus
        dlog/log_clock
        dlog/sink
        dlog/source
        dlog/span_scope

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
        dlog/detail/iso8601.hpp
        dlog/detail/utils.hpp
        dlog/detail/x_poly_types.inl

        dlog/detail/file_stream.hpp
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated/src/dplx/dlog/detail)
configure_file(tools/config.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/generated/src/dplx/dlog/detail/config.hpp @ONLY)
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
