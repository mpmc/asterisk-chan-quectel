#
# aarch64-none-linux-gnu.toolchain.cmake
#
# Download aarch64-none-linux-gnu toolchain from
# https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
# and extract to /build/arm-gnu-toolchain/
#
set(CMAKE_SYSTEM_NAME               Linux)
set(CMAKE_SYSTEM_PROCESSOR          aarch64)

set(triple                          aarch64-none-linux-gnu)
set(btriple                         aarch64-linux-gnu)
set(gccbase                         /build/arm-gnu-toolchain/${triple})

set(CMAKE_AR                        ${gccbase}/bin/${triple}-ar${CMAKE_EXECUTABLE_SUFFIX})
set(CMAKE_ASM_COMPILER              ${gccbase}/bin/${triple}-gcc${CMAKE_EXECUTABLE_SUFFIX})
set(CMAKE_C_COMPILER                ${gccbase}/bin/${triple}-gcc${CMAKE_EXECUTABLE_SUFFIX})
set(CMAKE_C_COMPILER_TARGET         ${triple})
set(CMAKE_CXX_COMPILER              ${gccbase}/bin/${triple}-g++${CMAKE_EXECUTABLE_SUFFIX})
set(CMAKE_CXX_COMPILER_TARGET       ${triple})
set(CMAKE_LINKER                    ${gccbase}/bin/${triple}-ld${CMAKE_EXECUTABLE_SUFFIX})
set(CMAKE_OBJCOPY                   ${gccbase}/bin/${triple}-objcopy${CMAKE_EXECUTABLE_SUFFIX} CACHE INTERNAL "")
set(CMAKE_RANLIB                    ${gccbase}/bin/${triple}-ranlib${CMAKE_EXECUTABLE_SUFFIX} CACHE INTERNAL "")
set(CMAKE_SIZE                      ${gccbase}/bin/${triple}-size${CMAKE_EXECUTABLE_SUFFIX} CACHE INTERNAL "")
set(CMAKE_STRIP                     ${gccbase}/bin/${triple}-strip${CMAKE_EXECUTABLE_SUFFIX} CACHE INTERNAL "")
set(CMAKE_GCOV                      ${gccbase}/bin/${triple}-gcov${CMAKE_EXECUTABLE_SUFFIX} CACHE INTERNAL "")

function(set_cxx_init_flags cflags)
    set(CMAKE_C_FLAGS_INIT "${cflags}" CACHE INTERNAL "")
    set(CMAKE_CXX_FLAGS_INIT "${cflags}" CACHE INTERNAL "")
endfunction()

set_cxx_init_flags("-march=armv8-a")

foreach(i C CXX)
    set("CMAKE_${i}_STANDARD_INCLUDE_DIRECTORIES"
       /usr/local/include/${btriple}
        /usr/local/include
        /usr/include/${btriple}
        /usr/include
    )
endforeach()

string(JOIN " " LINKER_FLAGS_INIT
    -Wl,-L/usr/local/lib/${btriple}
    -Wl,-L/usr/local/lib
    -Wl,-L/usr/lib/${btriple}
    -Wl,-L/usr/lib
    -fuse-ld=gold
)
foreach(i SHARED STATIC MODULE EXE)
    set("CMAKE_${i}_LINKER_FLAGS_INIT" "${LINKER_FLAGS_INIT}")
endforeach()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CPACK_PACKAGE_ARCHITECTURE arm64)
set(CMAKE_CROSSCOMPILING_EMULATOR /usr/bin/qemu-aarch64-static;-L;/usr/lib/${btriple} CACHE INTERNAL "")