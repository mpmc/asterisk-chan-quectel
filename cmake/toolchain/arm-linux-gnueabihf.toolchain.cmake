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

function(set_rpi_cxx_flags rpi)
    if (${rpi} EQUAL 1)
        set(CMAKE_C_FLAGS_INIT "-marm -mlibarch=armv6+fp -march=armv6+fp -mfpu=vfp -mfloat-abi=hard -mtune=arm1176jzf-s" CACHE INTERNAL "")
        set(CMAKE_CXX_FLAGS_INIT "-marm -mlibarch=armv6+fp -march=armv6+fp -mfpu=vfp -mfloat-abi=hard -mtune=arm1176jzf-s" CACHE INTERNAL "")
    elseif(${rpi} EQUAL 2)
        set(CMAKE_C_FLAGS_INIT "-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard -mtune=cortex-a7" CACHE INTERNAL "")
        set(CMAKE_CXX_FLAGS_INIT "-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard -mtune=cortex-a7" CACHE INTERNAL "")
    elseif(${rpi} EQUAL 3)
        set(CMAKE_C_FLAGS_INIT "-march=armv8-a+crc -mcpu=cortex-a53 -mfpu=crypto-neon-fp-armv8 -mfloat-abi=hard" CACHE INTERNAL "")
        set(CMAKE_CXX_FLAGS_INIT "-march=armv8-a+crc -mcpu=cortex-a53 -mfpu=crypto-neon-fp-armv8 -mfloat-abi=hard" CACHE INTERNAL "")
    elseif(${rpi} EQUAL 4)
        set(CMAKE_C_FLAGS_INIT "-march=armv8-a+crc -mcpu=cortex-a72 -mfpu=crypto-neon-fp-armv8 -mfloat-abi=hard" CACHE INTERNAL "")
        set(CMAKE_CXX_FLAGS_INIT "-march=armv8-a+crc -mcpu=cortex-a72 -mfpu=crypto-neon-fp-armv8 -mfloat-abi=hard" CACHE INTERNAL "")
    elseif(${rpi} EQUAL 5)
        set(CMAKE_C_FLAGS_INIT "-march=armv8-a+crc+crypto -mtune=cortex-a76 -mfpu=crypto-neon-fp-armv8 -mfloat-abi=hard" CACHE INTERNAL "")
        set(CMAKE_CXX_FLAGS_INIT "-march=armv8-a+crc+crypto -mtune=cortex-a76 -mfpu=crypto-neon-fp-armv8 -mfloat-abi=hard" CACHE INTERNAL "")        
    elseif(${rpi} GREATER 5)
        message(FATAL_ERROR "Version ${rpi} of Raspberry Pi is not supported.")
    else()
        message(FATAL_ERROR "Wrong Raspberry Pi version specified: ${rpi}.")
    endif()
endfunction()

if(DEFINED ENV{TOOLSET_TARGET_RPI})
    set_rpi_cxx_flags($ENV{TOOLSET_TARGET_RPI})
    set(CMAKE_FIND_ROOT_PATH        /build/rpi)
else()
    set(CMAKE_C_FLAGS_INIT          "-march=armv7-a -mfloat-abi=hard -mfpu=neon -Wno-psabi" CACHE INTERNAL "")
    set(CMAKE_CXX_FLAGS_INIT        "-march=armv7-a -mfloat-abi=hard -mfpu=neon -Wno-psabi" CACHE INTERNAL "")
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CPACK_PACKAGE_ARCHITECTURE armhf)