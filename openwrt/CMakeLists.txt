#
# OpenWRT
#

STRING(SUBSTRING ${ASTERISK_VERSION_NUM} 0 2 ASTERISK_MAJOR_VER)
IF(${ASTERISK_MAJOR_VER} LESS_EQUAL 16)
  SET(ASTERISK_PKG asterisk${ASTERISK_MAJOR_VER})
  SET(PKG_CMAKE_OPTIONS "CMAKE_OPTIONS += -DAST_HEADER_DIR=$(STAGING_DIR)/usr/include/asterisk-${ASTERISK_MAJOR_VER}/include")
  MESSAGE(STATUS "Package options: ${PKG_CMAKE_OPTIONS}")
  CONFIGURE_FILE(diffconfig.in diffconfig-gen @ONLY)
  INSTALL(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/diffconfig-gen
    DESTINATION openwrt
    COMPONENT openwrt EXCLUDE_FROM_ALL
    RENAME diffconfig
  )
ELSE()
  SET(ASTERISK_PKG asterisk)
  UNSET(PKG_CMAKE_OPTIONS)
  MESSAGE(DEBUG "Package options: none")
  INSTALL(
    FILES diffconfig
    DESTINATION openwrt
    COMPONENT openwrt EXCLUDE_FROM_ALL
  )
ENDIF()

INSTALL(
  FILES readme-package-building.txt
  DESTINATION openwrt
  COMPONENT openwrt EXCLUDE_FROM_ALL
  RENAME README.txt
)

INSTALL(
  PROGRAMS build-opk.sh
  DESTINATION openwrt
  COMPONENT openwrt EXCLUDE_FROM_ALL
)

SET(OPENWRT_FEED openwrt/feed/net/${PROJECT_NAME})

IF(OPENWRT_PACKAGE_STANDALONE)
  INSTALL(
    DIRECTORY ${CMAKE_SOURCE_DIR}/
    DESTINATION ${OPENWRT_FEED}/src
    COMPONENT openwrt EXCLUDE_FROM_ALL
    FILES_MATCHING
      PATTERN "*.txt"
      PATTERN "*.md"
      PATTERN "*.conf"
      PATTERN "*.c"
      PATTERN "*.h"
      PATTERN "*.in"
      PATTERN "*.cmake"
      PATTERN "pkg-vars.cmake.in" EXCLUDE
      PATTERN "build" EXCLUDE
      PATTERN "openwrt" EXCLUDE
      PATTERN "test" EXCLUDE
      PATTERN "tools" EXCLUDE
      PATTERN "install" EXCLUDE
      PATTERN "package" EXCLUDE
      PATTERN "etc" EXCLUDE
      PATTERN "uac" EXCLUDE
      PATTERN "doc" EXCLUDE
      PATTERN ".git" EXCLUDE
      PATTERN ".vscode" EXCLUDE
      PATTERN "make-*.cmake" EXCLUDE
      PATTERN "install-*.cmake" EXCLUDE
      PATTERN "build-*.cmake" EXCLUDE
      PATTERN "clang-*.cmake" EXCLUDE
      PATTERN ".clang-format" EXCLUDE
      PATTERN "diffconfig.in" EXCLUDE
  )

  CONFIGURE_FILE(pkg-vars.cmake.in pkg-vars.cmake @ONLY)

  INSTALL(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/pkg-vars.cmake
    DESTINATION ${OPENWRT_FEED}/src
    COMPONENT openwrt EXCLUDE_FROM_ALL
  )

  CONFIGURE_FILE(makefile-standalone.in Makefile-opkg @ONLY)

  INSTALL(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/Makefile-opkg
    DESTINATION ${OPENWRT_FEED}
    COMPONENT openwrt EXCLUDE_FROM_ALL
    RENAME Makefile
  )

ELSE()
  CONFIGURE_FILE(makefile-git.in Makefile-opkg @ONLY)

  INSTALL(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/Makefile-opkg
    DESTINATION ${OPENWRT_FEED}
    COMPONENT openwrt EXCLUDE_FROM_ALL
    RENAME Makefile
  )
ENDIF()

INSTALL(
  SCRIPT make-feeds-conf.cmake
  COMPONENT openwrt EXCLUDE_FROM_ALL
)
