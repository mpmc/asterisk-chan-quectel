#!/usr/bin/cmake -P

EXECUTE_PROCESS(
    COMMAND ${CMAKE_COMMAND} --build --preset default
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
