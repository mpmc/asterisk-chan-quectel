#
# aarch64-linux-gnu.toolchain.cmake
#
# Required packages:
#   gcc-aarch64-linux-gnu
#   g++-aarch64-linux-gnu
#   binutils-aarch64-linux-gnu
#
set(CMAKE_SYSTEM_NAME               Linux)
set(CMAKE_SYSTEM_PROCESSOR          aarch64)

set(triple                          arm-linux-aarch64)

set(CMAKE_AR                        aarch64-linux-gnu-ar${CMAKE_EXECUTABLE_SUFFIX})
set(CMAKE_ASM_COMPILER              aarch64-linux-gnu-gcc${CMAKE_EXECUTABLE_SUFFIX})
set(CMAKE_C_COMPILER                aarch64-linux-gnu-gcc${CMAKE_EXECUTABLE_SUFFIX})
set(CMAKE_C_COMPILER_TARGET         ${triple})
set(CMAKE_CXX_COMPILER              aarch64-linux-gnu-g++${CMAKE_EXECUTABLE_SUFFIX})
set(CMAKE_CXX_COMPILER_TARGET       ${triple})
set(CMAKE_LINKER                    aarch64-linux-gnu-ld${CMAKE_EXECUTABLE_SUFFIX})
set(CMAKE_OBJCOPY                   aarch64-linux-gnu-objcopy${CMAKE_EXECUTABLE_SUFFIX} CACHE INTERNAL "")
set(CMAKE_RANLIB                    aarch64-linux-gnu-ranlib${CMAKE_EXECUTABLE_SUFFIX} CACHE INTERNAL "")
set(CMAKE_SIZE                      aarch64-linux-gnu-size${CMAKE_EXECUTABLE_SUFFIX} CACHE INTERNAL "")
set(CMAKE_STRIP                     aarch64-linux-gnu-strip${CMAKE_EXECUTABLE_SUFFIX} CACHE INTERNAL "")
set(CMAKE_GCOV                      aarch64-linux-gnu-gcov${CMAKE_EXECUTABLE_SUFFIX} CACHE INTERNAL "")

set(CMAKE_C_FLAGS                   "-march=armv8-a" CACHE INTERNAL "")
set(CMAKE_CXX_FLAGS                 "-march=armv8-a" CACHE INTERNAL "")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CPACK_PACKAGE_ARCHITECTURE arm64)
