# syntax=docker/dockerfile:1

#
# Building chan_quectel on Debian 11 (Bullseye)
#

FROM debian:bullseye-slim AS base
ARG DEBIAN_FRONTEND=noninteractive

# update and install required packages
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked <<-EOF
#!/bin/bash -e

apt-get -qq update -y
apt-get -qq install -y --no-install-recommends \
    file build-essential \
    ca-certificates \
    libsqlite3-dev libasound2 libssl1.1 \
    libedit2 uuid libxml2 libjansson4
EOF

# make build directory
RUN --network=none <<-EOF
#!/bin/bash -e

mkdir -p /build/cache
mkdir -p /build/install
chown -R nobody:nogroup /build
EOF

FROM base as cmake

# Full version of CMake
ARG CMAKEVER=3.29.0

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked <<-EOF
#!/bin/bash -e

apt-get -qq update -y
apt-get -qq install -y --no-install-recommends libssl-dev
EOF

WORKDIR /build
USER nobody:nogroup

ADD --chown=nobody:nogroup \
    http://github.com/Kitware/CMake/releases/download/v${CMAKEVER}/cmake-${CMAKEVER}.tar.gz \
    .
RUN --network=none tar -xf cmake-${CMAKEVER}.tar.gz
WORKDIR /build/cmake-${CMAKEVER}
RUN --network=none ./bootstrap --prefix=/usr
RUN --network=none make -j$(nproc)
RUN --network=none DESTDIR=/build/install make install

FROM base as asterisk

# Only major version of Asterisk
ARG ASTVER=18

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked <<-EOF
#!/bin/bash -e

apt-get -qq update -y
apt-get -qq install -y --no-install-recommends \
    wget \
    libssl-dev libedit-dev uuid-dev libxml2-dev libjansson-dev
EOF

WORKDIR /build
USER nobody:nogroup

ADD --chown=nobody:nogroup \
    http://downloads.asterisk.org/pub/telephony/asterisk/asterisk-${ASTVER}-current.tar.gz \
    .

# extract Asterisk
RUN <<EOF
#!/bin/bash -e

tar -xf asterisk-${ASTVER}-current.tar.gz
ast=$(cd asterisk-${ASTVER}.*; pwd)
ln -sr ${ast} asterisk-${ASTVER}
EOF

WORKDIR /build/asterisk-${ASTVER}
COPY --chown=nobody:nogroup configure-asterisk.sh .
RUN --mount=type=cache,target=/build/cache,sharing=locked,uid=65534,gid=65534 ./configure-asterisk.sh
RUN --network=none make -j$(nproc) OPTIMIZE=-fmerge-constants
RUN --network=none DESTDIR=/build/install make install-headers

FROM base AS chan-quectel-base 

# git branch
ARG BRANCH=master

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked <<-EOF
#!/bin/bash -e

apt-get -qq update -y
apt-get -qq install -y --no-install-recommends git libasound2-dev
EOF

COPY --from=cmake --link /build/install/ /
COPY --from=asterisk --link /build/install/ /

WORKDIR /build
USER nobody:nogroup

RUN git clone -q --branch=${BRANCH} http://github.com/RoEdAl/asterisk-chan-quectel.git chan-quectel

USER root

FROM chan-quectel-base AS chan-quectel

WORKDIR /build/chan-quectel
USER nobody:nogroup
RUN --network=none cmake --preset=default
RUN --network=none cmake --build --preset=package

FROM chan-quectel-base AS chan-quectel-armhf

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked <<-EOF
#!/bin/bash -e

dpkg --add-architecture armhf
apt-get -qq update -y
apt-get -qq install -y --no-install-recommends \
    gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf binutils-arm-linux-gnueabihf \
    libsqlite3-dev:armhf libasound2-dev:armhf
EOF

WORKDIR /build/chan-quectel
USER nobody:nogroup
RUN --network=none cmake \
    -DASTERISK_VERSION_NUM=200100 \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/arm-linux-gnueabihf.toolchain.cmake \
    -DCPACK_PACKAGE_ARCHITECTURE=armhf \
    --preset=default 
RUN --network=none cmake --build --preset=package

FROM chan-quectel-base AS chan-quectel-arm64

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked <<-EOF
#!/bin/bash -e

dpkg --add-architecture arm64
apt-get -qq update -y
apt-get -qq install -y --no-install-recommends \
    gcc-aarch64-linux-gnu g++-aarch64-linux-gnu binutils-aarch64-linux-gnu \
    libsqlite3-dev:arm64 libasound2-dev:arm64
EOF

WORKDIR /build/chan-quectel
USER nobody:nogroup
RUN --network=none cmake \
    -DASTERISK_VERSION_NUM=200100 \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/aarch64-linux-gnu.toolchain.cmake \
    -DCPACK_PACKAGE_ARCHITECTURE=arm64 \
    --preset=default 
RUN --network=none cmake --build --preset=package
