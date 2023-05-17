#!/usr/bin/cmake -P

EXECUTE_PROCESS(
    COMMAND ${CMAKE_COMMAND} --preset default
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
