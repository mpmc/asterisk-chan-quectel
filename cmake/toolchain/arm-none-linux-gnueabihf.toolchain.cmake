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
    set(rpidir /build/rpi)
    set(CMAKE_FIND_ROOT_PATH ${rpidir})

    set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES 
        ${rpidir}/usr/include/${btriple}
        ${rpidir}/usr/include
        ${rpidir}/usr/local/include/${btriple}
        ${rpidir}/usr/local/include
    )
    set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES 
        ${rpidir}/usr/include/${btriple}
        ${rpidir}/usr/include
        ${rpidir}/usr/local/include/${btriple}
        ${rpidir}/usr/local/include
    )
    set(CMAKE_SHARED_LINKER_FLAGS_INIT
        -Wl,-L${rpidir}/usr/local/lib/${btriple}
        -Wl,-L${rpidir}/usr/local/lib
        -Wl,-L${rpidir}/usr/lib/${btriple}
        -Wl,-L${rpidir}/usr/lib
    )

    set(CMAKE_STATIC_LINKER_FLAGS_INIT
        -Wl,-L${rpidir}/usr/local/lib/${btriple}
        -Wl,-L${rpidir}/usr/local/lib
        -Wl,-L${rpidir}/usr/lib/${btriple}
        -Wl,-L${rpidir}/usr/lib
    )

    set(CMAKE_MODULE_LINKER_FLAGS_INIT
        -Wl,-L${rpidir}/usr/local/lib/${btriple}
        -Wl,-L${rpidir}/usr/local/lib
        -Wl,-L${rpidir}/usr/lib/${btriple}
        -Wl,-L${rpidir}/usr/lib
    )

    set(CMAKE_EXE_LINKER_FLAGS_INIT
        -Wl,-L${rpidir}/usr/local/lib/${btriple}
        -Wl,-L${rpidir}/usr/local/lib
        -Wl,-L${rpidir}/usr/lib/${btriple}
        -Wl,-L${rpidir}/usr/lib
    )    
else()
    set(CMAKE_C_FLAGS_INIT          "-march=armv7-a -mfloat-abi=hard -mfpu=neon" CACHE INTERNAL "")
    set(CMAKE_CXX_FLAGS_INIT        "-march=armv7-a -mfloat-abi=hard -mfpu=neon" CACHE INTERNAL "")

    set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES 
        /usr/include/${btriple}
        /usr/include
        /usr/local/include/${btriple}
        /usr/local/include
    )
    set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES 
        /usr/include/${btriple}
        /usr/include
        /usr/local/include/${btriple}
        /usr/local/include
    )
    set(CMAKE_SHARED_LINKER_FLAGS_INIT
        -Wl,-L/usr/local/lib/${btriple}
        -Wl,-L/usr/local/lib
        -Wl,-L/usr/lib/${btriple}
        -Wl,-L/usr/lib
    )

    set(CMAKE_STATIC_LINKER_FLAGS_INIT
        -Wl,-L/usr/local/lib/${btriple}
        -Wl,-L/usr/local/lib
        -Wl,-L/usr/lib/${btriple}
        -Wl,-L/usr/lib
    )

    set(CMAKE_MODULE_LINKER_FLAGS_INIT
        -Wl,-L/usr/local/lib/${btriple}
        -Wl,-L/usr/local/lib
        -Wl,-L/usr/lib/${btriple}
        -Wl,-L/usr/lib
    )

    set(CMAKE_EXE_LINKER_FLAGS_INIT
        -Wl,-L/usr/local/lib/${btriple}
        -Wl,-L/usr/local/lib
        -Wl,-L/usr/lib/${btriple}
        -Wl,-L/usr/lib
    )
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CPACK_PACKAGE_ARCHITECTURE armhf)
