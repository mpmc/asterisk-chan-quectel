#!/bin/bash -e

BUILD_TARGET=$(basename $0)
BUILD_TARGET=${BUILD_TARGET%.sh}
BUILD_TARGET=${BUILD_TARGET#build}

readonly IMGTAG=asterisk-chan-quectel/debian-bookworm:v1

docker image build \
    --build-arg BRANCH=$(git branch --show-current) \
    --tag=$IMGTAG \
    --target=chan-quectel${BUILD_TARGET} \
    .

CNTID=$(docker container create $IMGTAG)
docker container cp $CNTID:/build/chan-quectel/package/ package || true
docker container rm $CNTID
