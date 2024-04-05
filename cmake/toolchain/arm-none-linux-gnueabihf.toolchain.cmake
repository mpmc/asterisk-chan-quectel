#
# arm-none-linux-gnueabihf.toolchain.cmake
#
# Download arm-none-linux-gnueabihf toolchain from
# https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
# and extract to /build/arm-gnu-toolchain/
#
set(CMAKE_SYSTEM_NAME               Linux)
set(CMAKE_SYSTEM_PROCESSOR          arm)

set(triple                          arm-none-linux-gnueabihf)
set(btriple                         arm-linux-gnueabihf)
set(gccbase                          /build/arm-gnu-toolchain/${triple})

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

function(set_rpi_cxx_init_flags rpi)
    if (${rpi} EQUAL 1)
        set_cxx_init_flags("-marm -march=armv6+fp -mfpu=vfp -mfloat-abi=hard -mtune=arm1176jzf-s")
    elseif(${rpi} EQUAL 2)
        set_cxx_init_flags("-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard -mtune=cortex-a7")
    elseif(${rpi} EQUAL 3)
        set_cxx_init_flags("-march=armv8-a+crc -mcpu=cortex-a53 -mfpu=crypto-neon-fp-armv8 -mfloat-abi=hard")
    elseif(${rpi} EQUAL 4)
        set_cxx_init_flags("-march=armv8-a+crc -mcpu=cortex-a72 -mfpu=crypto-neon-fp-armv8 -mfloat-abi=hard")
    elseif(${rpi} EQUAL 5)
        set_cxx_init_flags("-march=armv8-a+crc+crypto -mtune=cortex-a76 -mfpu=crypto-neon-fp-armv8 -mfloat-abi=hard")
    elseif(${rpi} GREATER 5)
        message(FATAL_ERROR "Version ${rpi} of Raspberry Pi is not supported.")
    else()
        message(FATAL_ERROR "Wrong Raspberry Pi version specified: ${rpi}.")
    endif()
endfunction()

if(DEFINED ENV{TOOLSET_TARGET_RPI})
    set_rpi_cxx_init_flags($ENV{TOOLSET_TARGET_RPI})
    set(rpidir /build/rpi)
    set(CMAKE_FIND_ROOT_PATH ${rpidir})

    foreach(i C CXX)
        set("CMAKE_${i}_STANDARD_INCLUDE_DIRECTORIES"
            ${rpidir}/usr/include/${btriple}
            ${rpidir}/usr/include
            ${rpidir}/usr/local/include/${btriple}
            ${rpidir}/usr/local/include
        )
    endforeach()

    foreach(i SHARED STATIC MODULE EXE)
        set("CMAKE_${i}_LINKER_FLAGS_INIT"
            -Wl,-L${rpidir}/usr/local/lib/${btriple}
            -Wl,-L${rpidir}/usr/local/lib
            -Wl,-L${rpidir}/usr/lib/${btriple}
            -Wl,-L${rpidir}/usr/lib
        )
    endforeach()
    set(CMAKE_CROSSCOMPILING_EMULATOR /usr/bin/qemu-arm-static;-L;${rpidir}/usr/lib/armhf-linux-gnu CACHE INTERNAL "")
else()
    set_cxx_init_flags("-march=armv7-a -mfloat-abi=hard -mfpu=neon")

    foreach(i C CXX)
        set("CMAKE_${i}_STANDARD_INCLUDE_DIRECTORIES"
            /usr/local/include/${btriple}
            /usr/local/include
            /usr/include/${btriple}
            /usr/include
        )
    endforeach()

    foreach(i SHARED STATIC MODULE EXE)
        set("CMAKE_${i}_LINKER_FLAGS_INIT"
            -Wl,-L/usr/local/lib/${btriple}
            -Wl,-L/usr/local/lib
            -Wl,-L/usr/lib/${btriple}
            -Wl,-L/usr/lib
        )
    endforeach()
    set(CMAKE_CROSSCOMPILING_EMULATOR /usr/bin/qemu-arm-static;-L;/usr/lib/armhf-linux-gnu CACHE INTERNAL "")
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CPACK_PACKAGE_ARCHITECTURE armhf)