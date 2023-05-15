#!/bin/bash -e

./scripts/feeds update -a
./scripts/feeds install -a
./scripts/feeds install asterisk-chan-quectel


cp diffconfig .config
make defconfig
make package/asterisk-chan-quectel/compile
make package/index

