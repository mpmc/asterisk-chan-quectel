# syntax=docker/dockerfile:1

#
# Building chan_quectel on Raspbian 10 (Buster)
#

FROM --platform=linux/arm/v6 balenalib/rpi-debian:bullseye-build AS base
ARG DEBIAN_FRONTEND=noninteractive

# update and install required packages
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked <<-EOF
#!/bin/bash -e

apt-get -qq update -y
apt-get -qq install -y --no-install-recommends \
    file ca-certificates jq \
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
ARG SKIPCACHE_URL=https://www.random.org/cgi-bin/randbyte?nbytes=10&format=h

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked <<-EOF
#!/bin/bash -e

apt-get -qq update -y
apt-get -qq install -y --no-install-recommends \
    dpkg-dev \
    libasound2-dev asterisk-dev
EOF

COPY --from=cmake --link /build/install/ /

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
cmake -DASTERISK_VERSION_NUM=162800 --preset=deb
cmake --build --preset=package-deb
EOF
