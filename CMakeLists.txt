#
# asterisk-chan-quectel
#

CMAKE_MINIMUM_REQUIRED(VERSION 3.14)

SET(ASTERISK_VERSION_NUM 180000 CACHE STRING "Asterisk version")
OPTION(ICONV_CONST "iconv() const declaration of input buffer" OFF)
SET(ICONV_T iconv_t CACHE STRING "iconv type")
SET(AST_MODULE chan_quectel)

INCLUDE(CheckIncludeFile)
INCLUDE(CheckIncludeFiles)
INCLUDE(CheckSymbolExists)

PROJECT(chan-quectel
    VERSION 1.3.0.0
    HOMEPAGE_URL "http://github.com/RoEdAl/asterisk-chan-quectel"
    DESCRIPTION "Channel driver for Quectel modems"
    LANGUAGES C
)

SET(CMAKE_REQUIRED_DEFINITIONS "-DAST_MODULE_SELF_SYM=__internal_chan_quectel_self")
CHECK_INCLUDE_FILE(asterisk.h HAVE_ASTERISK_H)

IF(NOT HAVE_ASTERISK_H)
    MESSAGE(FATAL_ERROR "Asterisk header not found. Cannot continue.")
ENDIF()

CHECK_INCLUDE_FILES("stdarg.h;asterisk.h;asterisk/frame.h" HAVE_ASTERISK_FRAME_H)
CHECK_SYMBOL_EXISTS(AST_CONTROL_SRCCHANGE "stdarg.h;asterisk.h;asterisk/frame.h" HAVE_AST_CONTROL_SRCCHANGE)

CHECK_INCLUDE_FILE(fcntl.h HAVE_FCNTL_H)
CHECK_INCLUDE_FILE(inttypes.h HAVE_INTTYPES_H)

CHECK_INCLUDE_FILE(memory.h HAVE_MEMORY_H)
CHECK_SYMBOL_EXISTS(memchr memory.h HAVE_MEMCHR)
CHECK_SYMBOL_EXISTS(memmove memory.h HAVE_MEMMOVE)
CHECK_SYMBOL_EXISTS(memset memory.h HAVE_MEMSET)

CHECK_INCLUDE_FILE(stdint.h HAVE_STDINT_H)

CHECK_INCLUDE_FILE(stdlib.h HAVE_STDLIB_H)
CHECK_SYMBOL_EXISTS(realpath stdlib.h HAVE_REALPATH)
CHECK_SYMBOL_EXISTS(strtol stdlib.h HAVE_STRTOL)

CHECK_INCLUDE_FILE(string.h HAVE_STRING_H)
CHECK_SYMBOL_EXISTS(memmem string.h HAVE_MEMMEM)

CHECK_INCLUDE_FILE(strings.h HAVE_STRINGS_H)
CHECK_SYMBOL_EXISTS(strcasecmp strings.h HAVE_STRCASECMP)
CHECK_SYMBOL_EXISTS(strncasecmp strings.h HAVE_STRNCASECMP)

CHECK_INCLUDE_FILE(sys/stat.h HAVE_SYS_STAT_H)
CHECK_INCLUDE_FILE(sys/time.h HAVE_SYS_TIME_H)
CHECK_INCLUDE_FILE(sys/types.h HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILE(termios.h HAVE_TERMIOS_H)
CHECK_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)

SET(THREADS_PREFER_PTHREAD_FLAG ON)
FIND_PACKAGE(Threads REQUIRED)
FIND_PACKAGE(ALSA REQUIRED)
FIND_PACKAGE(SQLite3 REQUIRED)
FIND_PACKAGE(Iconv REQUIRED)

SET(SOURCES
    at_command.h at_command.c
    at_parse.h at_parse.c
    at_queue.h at_queue.c
    at_read.h at_read.c
    at_response.h at_response.c
    chan_quectel.h chan_quectel.c
    channel.h channel.c
    char_conv.h char_conv.c
    cli.h cli.c
    helpers.h helpers.c
    memmem.h memmem.c
    ringbuffer.h ringbuffer.c
    cpvt.h cpvt.c
    dc_config.h dc_config.c
    pdu.h pdu.c
    mixbuffer.h mixbuffer.c
    pdiscovery.h pdiscovery.c
    error.h error.c
    smsdb.h smsdb.c
)

IF(ICONV_CONST)
    SET(ICONV_CONST_STR "const")
ELSE()
    SET(ICONV_CONST_STR "/**/")
ENDIF()

IF(HAVE_AST_CONTROL_SRCCHANGE)
    SET(HAVE_AST_CONTROL_SRCCHANGE_STR 1)
ELSE()
    SET(HAVE_AST_CONTROL_SRCCHANGE_STR "/**/")
ENDIF()

CONFIGURE_FILE(config.h.in include/config.h)

ADD_LIBRARY(asterisk-chan-quectel SHARED ${SOURCES})
TARGET_COMPILE_FEATURES(asterisk-chan-quectel PUBLIC c_std_99)

TARGET_INCLUDE_DIRECTORIES(asterisk-chan-quectel BEFORE PRIVATE ${CMAKE_BINARY_DIR}/include)

TARGET_COMPILE_DEFINITIONS(asterisk-chan-quectel PRIVATE 
    _GNU_SOURCE
    AST_MODULE_SELF_SYM=__internal_chan_quectel_self
    HAVE_CONFIG_H
)

TARGET_LINK_LIBRARIES(asterisk-chan-quectel PRIVATE
    Threads::Threads
    ALSA::ALSA
    SQLite::SQLite3
    Iconv::Iconv
)

SET_TARGET_PROPERTIES(asterisk-chan-quectel PROPERTIES
    LIBRARY_OUTPUT_NAME ${AST_MODULE}
    PREFIX ""
    C_VISIBILITY_PRESET hidden
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

TARGET_COMPILE_OPTIONS(asterisk-chan-quectel PRIVATE
    $<$<AND:$<C_COMPILER_ID:GNU>,$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,8>>:-ffile-prefix-map=${CMAKE_SOURCE_DIR}=. -ffile-prefix-map=${CMAKE_BINARY_DIR}=.>            
)

INCLUDE(GNUInstallDirs)
INSTALL(TARGETS asterisk-chan-quectel DESTINATION ${CMAKE_INSTALL_LIBDIR}/asterisk/modules)
INSTALL(FILES quectel.conf DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/asterisk)



