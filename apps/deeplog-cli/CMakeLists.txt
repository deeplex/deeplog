#[=====================================================================[

Written in 2021 by Henrik Steffen Gaßmann <henrik@gassmann.onl>

To the extent possible under law, the author(s) have dedicated all
copyright and related and neighboring rights to this software to the
public domain worldwide. This software is distributed without any warranty.

You should have received a copy of the CC0 Public Domain Dedication
along with this software. If not, see

    http://creativecommons.org/publicdomain/zero/1.0/

#]=====================================================================]

# deeplog-cli library
add_library(deeplog-cli)
add_library(Deeplex::deeplog-cli ALIAS deeplog-cli)
target_include_directories(deeplog-cli
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(deeplog-cli PUBLIC
    Deeplex::deeplog
    Deeplex::deeplog_compiler_settings

    fmt::fmt
    ftxui::component
    ftxui::dom
    ftxui::screen
)

target_sources(deeplog-cli PRIVATE
    "include/dplx/dlog/tui/theme.hpp"
    "src/dlog/theme.cpp"

    "include/dplx/dlog/tui/log_display_grid.hpp"
    "src/dlog/log_display_grid.cpp"
)

# deeplog-cli executable utility
add_executable(deeplog-cli-main)
add_executable(Deeplex::deeplog-cli-main ALIAS deeplog-cli-main)
set_target_properties(deeplog-cli-main PROPERTIES
    OUTPUT_NAME deeplog
)
target_link_libraries(deeplog-cli-main PUBLIC
    Deeplex::deeplog
    Deeplex::deeplog-cli
    Deeplex::deeplog_compiler_settings
)

target_sources(deeplog-cli-main PRIVATE
    src/deeplog.main.cpp
)

########################################################################
# test target
if (BUILD_TESTING)
    add_executable(deeplog-cli-tests)
    target_link_libraries(deeplog-cli-tests PRIVATE
        Deeplex::deeplog-cli

        Boost::unit_test_framework
        fmt::fmt
    )

    target_sources(deeplog-cli-tests PRIVATE
        "tests/deeplog-cli-tests.main.cpp"
        "tests/boost-test.hpp"
    )

    add_test(NAME deeplog-cli-tests COMMAND deeplog-cli-tests)
endif()
