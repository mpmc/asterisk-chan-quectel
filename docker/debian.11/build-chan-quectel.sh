#!/bin/bash -e

readonly IMGTAG=asterisk-chan-quectel/debian-bullseye:v1

docker image build \
    --build-arg BRANCH=$(git branch --show-current) \
    --tag=$IMGTAG \
    --target=chan-quectel \
    .

CNTID=$(docker container create $IMGTAG)
docker container cp $CNTID:/build/chan-quectel/package/ package || true
docker container rm $CNTID
