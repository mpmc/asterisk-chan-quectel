#
# OpenWRT
#

INSTALL(
  FILES diffconfig
  DESTINATION openwrt
  COMPONENT openwrt EXCLUDE_FROM_ALL
)

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
      PATTERN "*.conf"
      PATTERN "*.c"
      PATTERN "*.h"
      PATTERN "*.in"
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
