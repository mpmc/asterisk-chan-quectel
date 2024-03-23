# syntax=docker/dockerfile:1

#
# Building chan_quectel on Raspbian 10 (Buster)
#

FROM --platform=linux/arm/v6 balenalib/rpi-debian:buster-build AS base
ARG DEBIAN_FRONTEND=noninteractive

# update and install required packages
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked <<-EOF
#!/bin/bash -e

apt-get -qq update -y
apt-get -qq install -y --no-install-recommends \
    file ca-certificates \
    libsqlite3-dev libssl1.1
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

FROM base as chan-quectel-base

ARG BRANCH=master

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked <<-EOF
#!/bin/bash -e

apt-get -qq update -y
apt-get -qq install -y --no-install-recommends \
    libasound2-dev asterisk-dev
EOF

COPY --from=cmake --link /build/install/ /

WORKDIR /build
USER nobody:nogroup
RUN git clone -q --branch=${BRANCH} http://github.com/RoEdAl/asterisk-chan-quectel.git chan-quectel

USER root

FROM chan-quectel-base AS chan-quectel

WORKDIR /build/chan-quectel
USER nobody:nogroup
RUN --network=none cmake -DASTERISK_VERSION_NUM=162800 --preset=default
RUN --network=none cmake --build --preset=package
