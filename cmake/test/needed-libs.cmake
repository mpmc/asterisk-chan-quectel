#
# needed-libs
# Check libraries required by module (dynamic library)
#
CMAKE_MINIMUM_REQUIRED(VERSION 3.20)

SET(ALLOWED_LIBS libasound libsqlite3 libc libpthread libgcc_s)

FUNCTION(GetNeededLib NEEDED_LIB)
    STRING(REGEX REPLACE "^\\[" "" NEEDED_LIB1 ${NEEDED_LIB})
    STRING(REGEX REPLACE "\\.so(\\.[0123456789]+)*\\]$" "" NEEDED_LIB2 ${NEEDED_LIB1})
    SET(NEEDED_LIB ${NEEDED_LIB2} PARENT_SCOPE)
ENDFUNCTION()

FUNCTION(CheckLibrary LIB_NAME OUT_VAR)
    IF(${LIB_NAME} IN_LIST ALLOWED_LIBS)
        SET("${OUT_VAR}_RESULT" ${LIB_NAME} PARENT_SCOPE)
        RETURN()
    ELSEIF(${LIB_NAME} MATCHES "^ld\\-linux\\-")
        SET("${OUT_VAR}_RESULT" ${LIB_NAME} PARENT_SCOPE)
        RETURN()
    ENDIF()

    SET("${OUT_VAR}_RESULT" PARENT_SCOPE)
ENDFUNCTION()

IF(NOT DEFINED CMAKE_READELF)
    MESSAGE(FATAL_ERROR "readelf utility not specified")
ENDIF()

IF(NOT DEFINED MODULE_PATH)
    MESSAGE(FATAL_ERROR "Library not specified")
ENDIF()

EXECUTE_PROCESS(
    COMMAND ${CMAKE_READELF} -d -W ${MODULE_PATH}
    OUTPUT_VARIABLE NEEDED_LIBRARIES_NL
    OUTPUT_STRIP_TRAILING_WHITESPACE    
    COMMAND_ERROR_IS_FATAL ANY
    TIMEOUT 15
)

SET(EXPECTED_LIBS)
SET(UNEXPECTED_LIBS)
STRING(REGEX MATCHALL "[^\n\r]+" NEEDED_LIBRARIES ${NEEDED_LIBRARIES_NL})
FOREACH(l IN LISTS NEEDED_LIBRARIES)
    IF(NOT ${l} MATCHES "NEEDED")
        CONTINUE()
    ENDIF()
    SEPARATE_ARGUMENTS(DLLDPL NATIVE_COMMAND "${l}")
    LIST(GET DLLDPL 4 NEEDED_LIB)
    GetNeededLib(${NEEDED_LIB})
    CheckLibrary(${NEEDED_LIB} LIB_CHECK)
    IF(LIB_CHECK_RESULT)
        LIST(APPEND EXPECTED_LIBS ${LIB_CHECK_RESULT})
        MESSAGE(STATUS "[lib] ${LIB_CHECK_RESULT} ✓")
    ELSE()
        LIST(APPEND UNEXPECTED_LIBS ${NEEDED_LIB})
        MESSAGE(STATUS "[lib] ${NEEDED_LIB} ⍻")
    ENDIF()
ENDFOREACH()

LIST(LENGTH UNEXPECTED_LIBS UNEXPECTED_LIBS_CNT)
LIST(LENGTH EXPECTED_LIBS EXPECTED_LIBS_CNT)
MESSAGE(STATUS "[status] expected:${EXPECTED_LIBS_CNT}, unexpected:${UNEXPECTED_LIBS_CNT}")

IF(${UNEXPECTED_LIBS_CNT} GREATER 0)
    MESSAGE(FATAL_ERROR "Unexpected libraries found")
ENDIF()

IF(${EXPECTED_LIBS_CNT} EQUAL 0)
    MESSAGE(FATAL_ERROR "Internal error — zero libraries checked")
ENDIF()
