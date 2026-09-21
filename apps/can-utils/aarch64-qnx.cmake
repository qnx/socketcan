# Taken from: https://github.com/libsdl-org/SDL/issues/8321

set(CMAKE_SYSROOT $ENV{QNX_TARGET})
set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_VERSION 800)
set(CMAKE_SYSTEM_PROCESSOR "aarch64")
set(arch nto${CMAKE_SYSTEM_PROCESSOR})

set(CMAKE_AS "$ENV{QNX_HOST}/usr/bin/${arch}-as")
set(CMAKE_AR "$ENV{QNX_HOST}/usr/bin/${arch}-ar")
set(CMAKE_C_COMPILER "$ENV{QNX_HOST}/usr/bin/qcc")
set(CMAKE_CXX_COMPILER "$ENV{QNX_HOST}/usr/bin/q++")
set(CMAKE_LINKER "$ENV{QNX_HOST}/usr/bin/${arch}-ld")

set(CMAKE_C_COMPILER_TARGET "gcc_ntoaarch64le")
set(CMAKE_CXX_COMPILER_TARGET "gcc_ntoaarch64le")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fexceptions -fPIC -D_XOPEN_SOURCE=600")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -fPIC -D_XOPEN_SOURCE=600 -std=gnu++14")

add_definitions(-D_POSIX_C_SOURCE=200112L -D_QNX_SOURCE)

set(CMAKE_FIND_LIBRARY_PREFIXES "lib")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".so;.a")

set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT};${CMAKE_SYSROOT}/aarch64le")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)


# Suggested in thread to disable using pkg-config to find stuff
set(ENV{PKG_CONFIG_LIBDIR} "/do_not_use_pkg_config_when_cross_compiling")
set(ENV{PKG_CONFIG_PATH} "/do_not_use_pkg_config_when_cross_compiling")

# Required to get CMake to correctly identify implicit include paths
set(CMAKE_C_VERBOSE_COMPILE_FLAG "-v -Wc,-v")
set(CMAKE_CXX_VERBOSE_COMPILE_FLAG "-v -Wc,-v")

# Link against libsocket
set(CMAKE_C_STANDARD_LIBRARIES "-lsocket -Wl,-Bstatic -lepoll -Wl,-Bdynamic")
