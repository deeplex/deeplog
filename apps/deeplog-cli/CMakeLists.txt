#[=====================================================================[

Written in 2021 by Henrik Steffen Gaßmann <henrik@gassmann.onl>

To the extent possible under law, the author(s) have dedicated all
copyright and related and neighboring rights to this software to the
public domain worldwide. This software is distributed without any warranty.

You should have received a copy of the CC0 Public Domain Dedication
along with this software. If not, see

    http://creativecommons.org/publicdomain/zero/1.0/

#]=====================================================================]

# deeplog-cli executable utility
add_executable(deeplog-cli-main)
add_executable(Deeplex::deeplog-cli-main ALIAS deeplog-cli-main)
set_target_properties(deeplog-cli-main PROPERTIES
    OUTPUT_NAME deeplog
)
target_link_libraries(deeplog-cli-main PUBLIC
    Deeplex::deeplog
    Deeplex::deeplog_tui
    Deeplex::deeplog_compiler_settings
)

target_sources(deeplog-cli-main PRIVATE
    deeplog.main.cpp
)
