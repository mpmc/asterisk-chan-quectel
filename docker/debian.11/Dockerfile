# syntax=docker/dockerfile:1

#
# Building chan_quectel on Debian 11 (Bullseye)
#

FROM debian:bullseye-slim AS arm-gnu-toolchain

ARG ARM_GNU_TOOLCHAIN_VER=13.2.rel1

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked <<-EOF
#!/bin/bash -e

apt-get -qq update -y
apt-get -qq install -y --no-install-recommends xz-utils
EOF

RUN --network=none <<-EOF
#!/bin/bash -e

mkdir -p /build/arm-gnu-toolchain
chown -R nobody:nogroup /build
EOF

FROM arm-gnu-toolchain AS arm-gnu-toolchain-gnueabihf

USER nobody:nogroup
WORKDIR /build/arm-gnu-toolchain
ADD --chown=nobody:nogroup \
    http://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/${ARM_GNU_TOOLCHAIN_VER}/binrel/arm-gnu-toolchain-${ARM_GNU_TOOLCHAIN_VER}-x86_64-arm-none-linux-gnueabihf.tar.xz \
    .
RUN --network=none <<-EOF
#!/bin/bash -e

tar -xf arm-gnu-toolchain-${ARM_GNU_TOOLCHAIN_VER}-x86_64-arm-none-linux-gnueabihf.tar.xz
rm arm-gnu-toolchain-${ARM_GNU_TOOLCHAIN_VER}-x86_64-arm-none-linux-gnueabihf.tar.xz
ln -s arm-gnu-toolchain-*-x86_64-arm-none-linux-gnueabihf arm-none-linux-gnueabihf
EOF

FROM arm-gnu-toolchain AS arm-gnu-toolchain-aarch64

USER nobody:nogroup
WORKDIR /build/arm-gnu-toolchain
ADD --chown=nobody:nogroup \
    http://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/${ARM_GNU_TOOLCHAIN_VER}/binrel/arm-gnu-toolchain-${ARM_GNU_TOOLCHAIN_VER}-x86_64-aarch64-none-linux-gnu.tar.xz \
    .
RUN --network=none <<-EOF
#!/bin/bash -e

tar -xf arm-gnu-toolchain-${ARM_GNU_TOOLCHAIN_VER}-x86_64-aarch64-none-linux-gnu.tar.xz
rm arm-gnu-toolchain-${ARM_GNU_TOOLCHAIN_VER}-x86_64-aarch64-none-linux-gnu.tar.xz
ln -s arm-gnu-toolchain-*-x86_64-aarch64-none-linux-gnu aarch64-none-linux-gnu
EOF

FROM debian:bullseye-slim AS base
ARG DEBIAN_FRONTEND=noninteractive

# update and install required packages
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked <<-EOF
#!/bin/bash -e

apt-get -qq update -y
apt-get -qq install -y --no-install-recommends \
    file build-essential \
    ca-certificates jq \
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
ARG BRANCH=master
ARG SKIPCACHE_URL=https://www.random.org/cgi-bin/randbyte?nbytes=10&format=h

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked <<-EOF
#!/bin/bash -e

apt-get -qq update -y
apt-get -qq install -y --no-install-recommends dpkg-dev git libasound2-dev
EOF

COPY --from=cmake --link /build/install/ /
COPY --from=asterisk --link /build/install/ /

WORKDIR /build
COPY --chown=nobody:nogroup --chmod=750 <<EOF git-clone.sh
#!/bin/bash -e

git clone -q --branch=${BRANCH} http://github.com/RoEdAl/asterisk-chan-quectel.git chan-quectel
EOF

FROM chan-quectel-base AS chan-quectel

USER nobody:nogroup
WORKDIR /build
ADD ${SKIPCACHE_URL} skipcache
RUN ./git-clone.sh

WORKDIR /build/chan-quectel
RUN --network=none \
    --mount=type=tmpfs,target=/build/chan-quectel/build <<-EOF
#!/bin/bash -e

./get-build-flags.sh deb > CMakeUserPresets.json
cmake --preset=deb
cmake --build --preset=package-deb
EOF

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

USER nobody:nogroup
WORKDIR /build
ADD ${SKIPCACHE_URL} skipcache
RUN ./git-clone.sh

WORKDIR /build/chan-quectel
RUN --network=none \
    --mount=type=tmpfs,target=/build/chan-quectel/build <<-EOF
#!/bin/bash -e

./get-build-flags.sh deb > CMakeUserPresets.json
cmake --toolchain=cmake/toolchain/arm-linux-gnueabihf.toolchain.cmake --preset=deb
cmake --build --preset=package-deb
EOF

FROM chan-quectel-base AS chan-quectel-armhf-ext-toolchain

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked <<-EOF
#!/bin/bash -e

dpkg --add-architecture armhf
apt-get -qq update -y
apt-get -qq install -y --no-install-recommends libsqlite3-dev:armhf libasound2-dev:armhf
EOF

USER nobody:nogroup
WORKDIR /build
ADD ${SKIPCACHE_URL} skipcache
RUN ./git-clone.sh

WORKDIR /build/chan-quectel
RUN --network=none ./get-build-flags.sh deb > CMakeUserPresets.json
RUN --network=none \
    --mount=type=tmpfs,target=/build/chan-quectel/build \
    --mount=type=bind,from=arm-gnu-toolchain-gnueabihf,source=/build/arm-gnu-toolchain,target=/build/arm-gnu-toolchain <<-EOF
#!/bin/bash -e

cmake --toolchain=cmake/toolchain/arm-none-linux-gnueabihf.toolchain.cmake --preset=deb
cmake --build --preset=package-deb
EOF

FROM --platform=linux/arm/v6 balenalib/rpi-debian:bullseye-build AS rpi-debian

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked <<-EOF
#!/bin/bash -e

apt-get -qq update -y
apt-get -qq install -y --no-install-recommends libsqlite3-dev libasound2-dev asterisk-dev
EOF

FROM chan-quectel-base AS chan-quectel-rpi

ARG RPI_VERSION=1

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked <<-EOF
#!/bin/bash -e

apt-get -qq update -y
apt-get -qq install -y --no-install-recommends \
   gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf binutils-arm-linux-gnueabihf
EOF

USER nobody:nogroup
WORKDIR /build
ADD ${SKIPCACHE_URL} skipcache
RUN ./git-clone.sh

WORKDIR /build/chan-quectel
RUN --network=none ./get-build-flags.sh rpi > CMakeUserPresets.json
RUN --network=none \
    --mount=type=tmpfs,target=/build/chan-quectel/build \
    --mount=type=bind,from=rpi-debian,source=/,target=/build/rpi <<-EOF
#!/bin/bash -e

env TOOLSET_TARGET_RPI=${RPI_VERSION} \
    cmake --toolchain=cmake/toolchain/arm-linux-gnueabihf.toolchain.cmake --preset=deb
cmake --build --preset=package-deb
EOF

FROM chan-quectel-base AS chan-quectel-rpi-ext-toolchain

ARG RPI_VERSION=1

USER nobody:nogroup
WORKDIR /build
ADD ${SKIPCACHE_URL} skipcache
RUN ./git-clone.sh

WORKDIR /build/chan-quectel
RUN --network=none ./get-build-flags.sh rpi > CMakeUserPresets.json
RUN --network=none \
    --mount=type=tmpfs,target=/build/chan-quectel/build \
    --mount=type=bind,from=arm-gnu-toolchain-gnueabihf,source=/build/arm-gnu-toolchain,target=/build/arm-gnu-toolchain \
    --mount=type=bind,from=rpi-debian,source=/,target=/build/rpi <<-EOF
#!/bin/bash -e

env TOOLSET_TARGET_RPI=${RPI_VERSION} \
    cmake --toolchain=cmake/toolchain/arm-none-linux-gnueabihf.toolchain.cmake \
    --preset=deb
cmake --build --preset=package-deb
EOF

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

USER nobody:nogroup
WORKDIR /build
ADD ${SKIPCACHE_URL} skipcache
RUN ./git-clone.sh

WORKDIR /build/chan-quectel
RUN --network=none \
    --mount=type=tmpfs,target=/build/chan-quectel/build <<-EOF
#!/bin/bash -e

./get-build-flags.sh deb > CMakeUserPresets.json
cmake --toolchain=cmake/toolchain/aarch64-linux-gnu.toolchain.cmake --preset=deb
cmake --build --preset=package-deb
EOF

FROM chan-quectel-base AS chan-quectel-arm64-ext-toolchain

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked <<-EOF
#!/bin/bash -e

dpkg --add-architecture arm64
apt-get -qq update -y
apt-get -qq install -y --no-install-recommends libsqlite3-dev:arm64 libasound2-dev:arm64
EOF

USER nobody:nogroup
WORKDIR /build
ADD ${SKIPCACHE_URL} skipcache
RUN ./git-clone.sh

WORKDIR /build/chan-quectel
RUN --network=none ./get-build-flags.sh deb > CMakeUserPresets.json
RUN --network=none \
    --mount=type=tmpfs,target=/build/chan-quectel/build \
    --mount=type=bind,from=arm-gnu-toolchain-aarch64,source=/build/arm-gnu-toolchain,target=/build/arm-gnu-toolchain <<-EOF
#!/bin/bash -e

cmake \
    --toolchain=cmake/toolchain/aarch64-none-linux-gnu.toolchain.cmake --preset=deb
cmake --build --preset=package-deb
EOF
