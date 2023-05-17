#!/usr/bin/cmake -P

SET(ENV{DESTDIR} ${CMAKE_SOURCE_DIR}/install)
EXECUTE_PROCESS(
    COMMAND ${CMAKE_COMMAND} --install build --component openwrt --prefix=/
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
