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
add_library(deeplog_tui)
add_library(Deeplex::deeplog_tui ALIAS deeplog_tui)

target_link_libraries(deeplog_tui PUBLIC
    Deeplex::deeplog
    Deeplex::deeplog_compiler_settings

    fmt::fmt
    ftxui::component
    ftxui::dom
    ftxui::screen
)

target_include_directories(deeplog_tui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

########################################################################
# library test project
if (BUILD_TESTING)
    add_executable(deeplog_tui-tests)

    target_link_libraries(deeplog_tui-tests PRIVATE
        Deeplex::deeplog_tui

        Catch2::Catch2 Catch2::Catch2WithMain
    )

    target_include_directories(deeplog_tui-tests PRIVATE
        src/dlog_tui_tests
    )

    add_test(NAME deeplog_tui-tests COMMAND deeplog_tui-tests)
endif()

########################################################################
# sources

dplx_target_sources(deeplog_tui
    TEST_TARGET deeplog_tui-tests
    MODE SMART_SOURCE MERGED_LAYOUT
    BASE_DIR dplx

    PRIVATE
        dlog/tui

        dlog/tui/log_display_grid
        dlog/tui/theme
)

if (BUILD_TESTING)
    dplx_target_sources(deeplog_tui-tests PRIVATE
        MODE VERBATIM
        BASE_DIR dlog_tui_tests

        PRIVATE
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
