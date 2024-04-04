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
    file ca-certificates jq \
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
ARG BRANCH=master
ARG SKIPCACHE_URL=https://www.random.org/cgi-bin/randbyte?nbytes=10&format=h

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked <<-EOF
#!/bin/bash -e

apt-get -qq update -y
apt-get -qq install -y --no-install-recommends \
    cmake dpkg-dev \
    libasound2-dev asterisk-dev
EOF

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
cmake --preset=deb -DASTERISK_VERSION_NUM=200100
cmake --build --preset=package-deb
EOF
