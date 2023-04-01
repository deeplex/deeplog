#[=====================================================================[

Written in 2021 by Henrik Steffen Gaßmann <henrik@gassmann.onl>

To the extent possible under law, the author(s) have dedicated all
copyright and related and neighboring rights to this software to the
public domain worldwide. This software is distributed without any warranty.

You should have received a copy of the CC0 Public Domain Dedication
along with this software. If not, see

    http://creativecommons.org/publicdomain/zero/1.0/

#]=====================================================================]

# 01
add_executable(example-log-complete)
add_executable(Deeplex::example-log-complete ALIAS example-log-complete)
set_target_properties(example-log-complete PROPERTIES
    OUTPUT_NAME log-complete
)

target_link_libraries(example-log-complete PUBLIC
    Deeplex::deeplog
    Deeplex::deeplog_compiler_settings
)

target_sources(example-log-complete PRIVATE
    log-complete/log-complete.main.cpp
)

