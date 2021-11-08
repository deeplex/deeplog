find_program(SPHINX_EXECUTABLE
    NAMES sphinx-build
    DOC "path to sphinx-build"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(sphinx
    "Failed to find sphinx-build executable"
    SPHINX_EXECUTABLE
)
