set(CMAKE_SYSTEM_PROCESSOR arm64)

execute_process(COMMAND brew --prefix llvm@16 OUTPUT_VARIABLE BREW_LLVM_PATH OUTPUT_STRIP_TRAILING_WHITESPACE)

set(CMAKE_C_COMPILER "${BREW_LLVM_PATH}/bin/clang")
set(CMAKE_CXX_COMPILER "${BREW_LLVM_PATH}/bin/clang++")
set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0 CACHE STRING "OSX deployment target")
set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "OSX target architectures")
set(CMAKE_OSX_SYSROOT /Applications/Xcode_15.2.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk CACHE FILEPATH "The macos SDK to use")

set(CMAKE_C_STANDARD 17 CACHE STRING "C standard")
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard")

set(CMAKE_CXX_FLAGS_INIT "-fsized-deallocation")
