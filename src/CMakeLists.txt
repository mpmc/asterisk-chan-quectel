#
# asterisk-chan-quectel
#

SET(SOURCES
    at_command.c
    at_parse.c
    at_queue.c
    at_read.c
    at_response.c
    chan_quectel.c
    channel.c
    char_conv.c
    cli.c
    helpers.c
    memmem.c
    ringbuffer.c
    cpvt.c
    dc_config.c
    pdu.c
    mixbuffer.c
    pdiscovery.c
    error.c
    smsdb.c
)

SET(HEADERS
    at_command.h
    at_parse.h
    at_queue.h
    at_read.h
    at_response.h
    chan_quectel.h
    channel.h
    char_conv.h
    cli.h
    helpers.h
    memmem.h
    ringbuffer.h
    cpvt.h
    dc_config.h
    pdu.h
    mixbuffer.h
    pdiscovery.h
    error.h
    smsdb.h
)

ADD_LIBRARY(asterisk-chan-quectel SHARED ${SOURCES} ${HEADERS})
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
    $<$<AND:$<C_COMPILER_ID:GNU>,$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,8>>:-ffile-prefix-map=${CMAKE_CURRENT_BINARY_DIR}=bin -ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=src>
    $<$<AND:$<C_COMPILER_ID:GNU>,$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,4>>:-Wall>
)

INSTALL(TARGETS asterisk-chan-quectel DESTINATION ${CMAKE_INSTALL_LIBDIR}/asterisk/modules)