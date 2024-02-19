set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(CMAKE_C_COMPILER gcc-12)
set(CMAKE_CXX_COMPILER g++-12)
set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0 CACHE STRING "OSX deployment target")
set(CMAKE_OSX_ARCHITECTURES x86_64 CACHE STRING "OSX target architectures")
set(CMAKE_OSX_SYSROOT /Applications/Xcode_15.2.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk CACHE FILEPATH "The macos SDK to use")
# the new linker currently segfaults with gcc (I believe this is fixed with gcc 14)
set(CMAKE_CXX_FLAGS_INIT "-Wl,-ld_classic")

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
