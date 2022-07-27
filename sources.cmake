dplx_target_sources(deeplog
    TEST_TARGET deeplog-tests
    MODE SMART_SOURCE MERGED_LAYOUT
    BASE_DIR dplx

    PUBLIC
        dlog

        dlog/disappointment

        dlog/core
        dlog/log_clock
        dlog/sink

        dlog/record_container

        dlog/file_database

        dlog/detail/interleaving_stream
)

dplx_target_sources(deeplog
    MODE VERBATIM MERGED_LAYOUT
    BASE_DIR dplx

    PUBLIC
        dlog/concepts.hpp
        dlog/definitions.hpp

        dlog/arguments.hpp
        dlog/argument_transmorpher_fmt.hpp
        dlog/attribute_transmorpher.hpp
        dlog/attributes.hpp

        dlog/log_bus.hpp

        dlog/detail/codec_dummy.hpp
        dlog/detail/iso8601.hpp
        dlog/detail/utils.hpp

        dlog/detail/file_stream.hpp
)

if (BUILD_TESTING)
    dplx_target_sources(deeplog-tests PRIVATE
        MODE VERBATIM
        BASE_DIR dlog_tests

        PRIVATE
            test_utils.hpp
    )
endif ()
