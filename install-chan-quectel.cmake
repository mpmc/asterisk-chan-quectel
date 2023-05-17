#!/usr/bin/cmake -P

SET(ENV{DESTDIR} ${CMAKE_SOURCE_DIR}/install/chan-quectel)
EXECUTE_PROCESS(
    COMMAND ${CMAKE_COMMAND} --install build --component chan-quectel --prefix=/usr
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
