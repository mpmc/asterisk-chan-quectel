#
# arm-linux-gnueabihf.toolchain.cmake
#
# Required packages:
#   gcc-arm-linux-gnueabihf
#   g++-arm-linux-gnueabihf
#   binutils-arm-linux-gnueabihf
#
set(CMAKE_SYSTEM_NAME               Linux)
set(CMAKE_SYSTEM_PROCESSOR          arm)

set(triple                          arm-linux-gnueabihf)

set(CMAKE_AR                        arm-linux-gnueabihf-ar${CMAKE_EXECUTABLE_SUFFIX})
set(CMAKE_ASM_COMPILER              arm-linux-gnueabihf-gcc${CMAKE_EXECUTABLE_SUFFIX})
set(CMAKE_C_COMPILER                arm-linux-gnueabihf-gcc${CMAKE_EXECUTABLE_SUFFIX})
set(CMAKE_C_COMPILER_TARGET         ${triple})
set(CMAKE_CXX_COMPILER              arm-linux-gnueabihf-g++${CMAKE_EXECUTABLE_SUFFIX})
set(CMAKE_CXX_COMPILER_TARGET       ${triple})
set(CMAKE_LINKER                    arm-linux-gnueabihf-ld${CMAKE_EXECUTABLE_SUFFIX})
set(CMAKE_OBJCOPY                   arm-linux-gnueabihf-objcopy${CMAKE_EXECUTABLE_SUFFIX} CACHE INTERNAL "")
set(CMAKE_RANLIB                    arm-linux-gnueabihf-ranlib${CMAKE_EXECUTABLE_SUFFIX} CACHE INTERNAL "")
set(CMAKE_SIZE                      arm-linux-gnueabihf-size${CMAKE_EXECUTABLE_SUFFIX} CACHE INTERNAL "")
set(CMAKE_STRIP                     arm-linux-gnueabihf-strip${CMAKE_EXECUTABLE_SUFFIX} CACHE INTERNAL "")
set(CMAKE_GCOV                      arm-linux-gnueabihf-gcov${CMAKE_EXECUTABLE_SUFFIX} CACHE INTERNAL "")

function(set_cxx_init_flags cflags)
    set(CMAKE_C_FLAGS_INIT "${cflags}" CACHE INTERNAL "")
    set(CMAKE_CXX_FLAGS_INIT "${cflags}" CACHE INTERNAL "")
endfunction()

function(set_rpi_cxx_init_flags rpi)
    if (${rpi} EQUAL 1)
        set_cxx_init_flags("-marm -march=armv6+fp -mfpu=vfp -mfloat-abi=hard -mtune=arm1176jzf-s")
    elseif(${rpi} EQUAL 2)
        set_cxx_init_flags("-march=armv7-a+fp+neon-vfpv4 -mfloat-abi=hard -mtune=cortex-a7")
    elseif(${rpi} EQUAL 3)
        set_cxx_init_flags("-march=armv8-a+crc -mfloat-abi=hard -mfpu=crypto-neon-fp-armv8 -mtune=cortex-a53")
    elseif(${rpi} EQUAL 4)
        set_cxx_init_flags("-march=armv8-a+crc -mfloat-abi=hard -mfpu=crypto-neon-fp-armv8 -mtune=cortex-a72")
    elseif(${rpi} EQUAL 5)
        set_cxx_init_flags("-march=armv8-a+crc+crypto -mfloat-abi=hard -mfpu=crypto-neon-fp-armv8 -mtune=cortex-a76")
    elseif(${rpi} GREATER 5)
        message(FATAL_ERROR "Version ${rpi} of Raspberry Pi is not supported.")
    else()
        message(FATAL_ERROR "Wrong Raspberry Pi version specified: ${rpi}.")
    endif()
endfunction()

if(DEFINED ENV{TOOLSET_TARGET_RPI})
    set_rpi_cxx_init_flags($ENV{TOOLSET_TARGET_RPI})
    set(rpidir /build/rpi)
    set(CMAKE_SYSROOT ${rpidir})
    set(CMAKE_STAGING_PREFIX ${rpidir})

    # paths relative to SYSROOT
    foreach(i C CXX)
        set("CMAKE_${i}_STANDARD_INCLUDE_DIRECTORIES"
            "=/include/${btriple}"
            "=/include"
        )
    endforeach()
    set(CMAKE_CROSSCOMPILING_EMULATOR /usr/bin/qemu-arm-static;-L;${rpidir}/lib CACHE INTERNAL "")
else()
    set_cxx_init_flags("-march=armv7-a+fp+neon -mfloat-abi=hard")
    set(CMAKE_CROSSCOMPILING_EMULATOR /usr/bin/qemu-arm-static;-L;/usr/lib/${triple} CACHE INTERNAL "")    
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CPACK_PACKAGE_ARCHITECTURE armhf)
