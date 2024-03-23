# syntax=docker/dockerfile:1

#
# Building chan_quectel on RaspberryPiOS 12 (Bookworm)
#

FROM --platform=linux/arm/v6 balenalib/rpi-debian:bookworm-build AS base
ARG DEBIAN_FRONTEND=noninteractive

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked <<-EOF
#!/bin/bash -e

apt-get -qq update -y
apt-get -qq install -y --no-install-recommends \
    file ca-certificates \
    libsqlite3-dev libssl3
EOF

# make build directory
RUN --network=none <<-EOF
#!/bin/bash -e

mkdir -p /build/cache
mkdir -p /build/install
chown -R nobody:nogroup /build
EOF

FROM base AS chan-quectel-base

# git branch
ARG BRANCH=master

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked <<-EOF
#!/bin/bash -e

apt-get -qq update -y
apt-get -qq install -y --no-install-recommends \
    cmake \
    libasound2-dev asterisk-dev
EOF

WORKDIR /build
USER nobody:nogroup
RUN git clone -q --branch=${BRANCH} http://github.com/RoEdAl/asterisk-chan-quectel.git chan-quectel

USER root

FROM chan-quectel-base AS chan-quectel

WORKDIR /build/chan-quectel
USER nobody:nogroup
RUN --network=none cmake -DASTERISK_VERSION_NUM=200100 --preset=default
RUN --network=none cmake --build --preset=package
