#
# asterisk-chan-quectel
#

INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/source-files.cmake)

ADD_LIBRARY(asterisk-chan-quectel SHARED ${SOURCES} ${HEADERS})
TARGET_COMPILE_FEATURES(asterisk-chan-quectel PUBLIC c_std_99)
IF(CHECK_SOURCE_DATE_EPOCH)
    ADD_DEPENDENCIES(asterisk-chan-quectel check-source-date-epoch)
ENDIF()

TARGET_INCLUDE_DIRECTORIES(asterisk-chan-quectel BEFORE PRIVATE ${CMAKE_BINARY_DIR}/include)

TARGET_COMPILE_DEFINITIONS(asterisk-chan-quectel PRIVATE 
    _GNU_SOURCE
    HAVE_CONFIG_H
)

TARGET_LINK_LIBRARIES(asterisk-chan-quectel PRIVATE
    AsteriskModule
    Threads::Threads
    ALSA::ALSA
    SQLite::SQLite3
    Iconv::Iconv
)

SET_TARGET_PROPERTIES(asterisk-chan-quectel PROPERTIES
    LIBRARY_OUTPUT_NAME ${AST_MODULE}
    PREFIX ""
)

TARGET_COMPILE_OPTIONS(asterisk-chan-quectel PRIVATE
    $<$<AND:$<C_COMPILER_ID:GNU>,$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,8>>:-ffile-prefix-map=${CMAKE_CURRENT_BINARY_DIR}=bin -ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=src>
    $<$<AND:$<C_COMPILER_ID:GNU>,$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,4>>:-Wall>
)

INSTALL(
    TARGETS asterisk-chan-quectel
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/asterisk/modules
    COMPONENT chan-quectel
)

# install configuration file
INSTALL(
    FILES ${CMAKE_SOURCE_DIR}/quectel.conf
    DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/asterisk
    COMPONENT chan-quectel
)

IF(IS_GIT_REPO)
    IF(DEFINED CMAKE_READELF)
        ADD_TEST(NAME "Load library" COMMAND ${CMAKE_READELF} -h -W $<TARGET_FILE:asterisk-chan-quectel>)
        ADD_TEST(NAME "Check library dependencies" COMMAND ${CMAKE_COMMAND}
            -P ${CMAKE_SOURCE_DIR}/cmake/test/needed-libs.cmake
            ${CMAKE_READELF}
            $<TARGET_FILE:asterisk-chan-quectel>
        )
        ADD_TEST(NAME "Check comment section" COMMAND ${CMAKE_COMMAND}
            -P ${CMAKE_SOURCE_DIR}/cmake/test/comment-section.cmake
            ${CMAKE_READELF}
            $<TARGET_FILE:asterisk-chan-quectel>
            ${CMAKE_C_COMPILER_VERSION}
        )
        SET_TESTS_PROPERTIES(
            "Load library"
            "Check library dependencies"
            "Check comment section"
            PROPERTIES
                LABELS "DLL;READELF"
                TIMEOUT 20
                WORKING_DIRECTORY $<TARGET_FILE_DIR:asterisk-chan-quectel>
        )
    ELSE()
        MESSAGE(WARNING Cannot find readelf utility)
    ENDIF()
    ADD_TEST(NAME "Check AST_BUILDOPT_SUM" COMMAND ${CMAKE_COMMAND}
        -P ${CMAKE_SOURCE_DIR}/cmake/test/AST_BUILDOPT_SUM.cmake
        $<TARGET_FILE:asterisk-chan-quectel>
        ${AST_BUILDOPT_SUM}
    )
    SET_TESTS_PROPERTIES(
        "Check AST_BUILDOPT_SUM"
        PROPERTIES
            LABELS DLL
            TIMEOUT 20
            WORKING_DIRECTORY $<TARGET_FILE_DIR:asterisk-chan-quectel>
    )
ENDIF()

# formatting targets
IF(NOT CLANG_FORMAT)
    MESSAGE(WARNING "Cannot create formatting targets - clang-format not found")
ELSE()
    ADD_CUSTOM_TARGET(asterisk-chan-quectel-code-formatter
        COMMAND ${CLANG_FORMAT}
            -i
            --style=file
            $<TARGET_PROPERTY:asterisk-chan-quectel,SOURCES>
        WORKING_DIRECTORY
            $<TARGET_PROPERTY:asterisk-chan-quectel,SOURCE_DIR>
        COMMENT
            "Formatting code"
        COMMAND_EXPAND_LISTS
    )

    ADD_CUSTOM_TARGET(asterisk-chan-quectel-code-formatting-check
        COMMAND ${CLANG_FORMAT}
            --dry-run
            --verbose
            -i
            --style=file
            $<TARGET_PROPERTY:asterisk-chan-quectel,SOURCES>
        WORKING_DIRECTORY
            $<TARGET_PROPERTY:asterisk-chan-quectel,SOURCE_DIR>
        COMMENT
            "Checking code formatting"
        COMMAND_EXPAND_LISTS
    )

    # ADD_DEPENDENCIES(asterisk-chan-quectel asterisk-chan-quectel-code-formatter)
    # ADD_DEPENDENCIES(asterisk-chan-quectel asterisk-chan-quectel-code-formatting-check)
ENDIF()
