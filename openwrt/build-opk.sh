#!/bin/bash -e

PKG_NAME=asterisk-chan-quectel

if [ -f .config ]; then
    make package/${PKG_NAME}/clean
    make package/${PKG_NAME}/compile
    make package/index
else
    cat feeds-strskx.conf >> feeds.conf.default
    ./scripts/feeds update -a
    ./scripts/feeds install -a
    ./scripts/feeds install ${PKG_NAME}

    cp diffconfig .config
    make defconfig
    make package/${PKG_NAME}/compile
    make package/index
fi
