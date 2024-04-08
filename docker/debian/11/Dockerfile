# syntax=docker/dockerfile:1

#
# Building chan_quectel on Debian 11 (Bullseye)
#

ARG DEBIAN_FRONTEND=noninteractive
ARG CC_TARGET=armhf
ARG CC_TOOLCHAIN=arm-linux-gnueabihf
ARG ARM_GNU_TOOLCHAIN_VER=13.2.rel1
ARG CMAKEVER=3.29.2
ARG ASTVER=18

FROM scratch AS dl-task
ADD --chmod=444 http://github.com/go-task/task/releases/latest/download/task_linux_amd64.deb .

FROM debian:bullseye-slim AS task-base
ARG DEBIAN_FRONTEND
RUN --network=none mkdir /build
RUN --network=none --mount=type=bind,from=dl-task,target=/build/download dpkg --install /build/download/task_linux_amd64.deb
COPY --from=task --chown=nobody:nogroup Taskfile.dist.yaml /build/Taskfile.yaml

FROM scratch AS dl-arm-gnu-toolchain
ARG ARM_GNU_TOOLCHAIN_VER
ADD --chmod=444 http://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/${ARM_GNU_TOOLCHAIN_VER}/binrel/arm-gnu-toolchain-${ARM_GNU_TOOLCHAIN_VER}-x86_64-arm-none-linux-gnueabihf.tar.xz .
ADD --chmod=444 http://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/${ARM_GNU_TOOLCHAIN_VER}/binrel/arm-gnu-toolchain-${ARM_GNU_TOOLCHAIN_VER}-x86_64-aarch64-none-linux-gnu.tar.xz .

FROM task-base AS arm-gnu-toolchain
ARG ARM_GNU_TOOLCHAIN_VER

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    task -d /build install-pkgs -- xz-utils
RUN --network=none task -d /build arm-gnu-toolchain-create-directory

USER nobody:nogroup
RUN --network=none --mount=type=bind,from=dl-arm-gnu-toolchain,target=/build/download task -d /build arm-gnu-toolchain-prepare

FROM task-base AS base

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    task -d /build install-pkgs -- file build-essential dpkg-dev ca-certificates jq libsqlite3-dev libssl1.1
RUN --network=none task -d /build create-build-directory create-dotenv chown-build-directory -- nobody:nogroup

FROM scratch as dl-cmake
ARG CMAKEVER
ADD --chmod=444 http://github.com/Kitware/CMake/releases/download/v${CMAKEVER}/cmake-${CMAKEVER}.tar.gz .

FROM base as cmake
ARG CMAKEVER

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    task -d /build install-pkgs -- libssl-dev

USER nobody:nogroup
RUN --network=none --mount=type=bind,from=dl-cmake,target=/build/download task -d /build cmake-extract
RUN --network=none task -d /build cmake-bootstrap cmake-make cmake-install

FROM scratch AS dl-asterisk
ARG ASTVER 
ADD --chmod=444 http://downloads.asterisk.org/pub/telephony/asterisk/asterisk-${ASTVER}-current.tar.gz .

FROM base as asterisk
ARG ASTVER

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    task -d /build install-pkgs -- wget libssl-dev libedit-dev uuid-dev libxml2-dev
USER nobody:nogroup
RUN --network=none --mount=type=bind,from=dl-asterisk,target=/build/download task -d /build asterisk-extract-sources
RUN --mount=type=cache,target=/build/cache,sharing=locked,uid=65534,gid=65534 task -d /build asterisk-configure-${ASTVER}
RUN --network=none task -d /build asterisk-build asterisk-install-headers

FROM base as chan-quectel
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    task -d /build install-pkgs -- git libasound2-dev

COPY --from=cmake --link /build/install/ /
COPY --from=asterisk --link /build/install/ /

FROM chan-quectel AS chan-quectel-cc
ARG CC_TARGET
ARG CC_TOOLCHAIN

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    task -d /build install-pkgs-arch -- qemu-user-static \
    gcc-${CC_TOOLCHAIN} g++-${CC_TOOLCHAIN} binutils-${CC_TOOLCHAIN} \
    libsqlite3-dev:${CC_TARGET} libasound2-dev:${CC_TARGET}

FROM --platform=linux/arm/v6 balenalib/rpi-debian:bullseye-build AS rpi-debian

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked <<-EOF
#!/bin/bash -e

apt-get -qq update -y
    apt-get -qq install -y --no-install-recommends libsqlite3-dev libasound2-dev asterisk-dev
EOF

FROM chan-quectel AS chan-quectel-rpi
ARG CC_TARGET
ARG CC_TOOLCHAIN

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    task -d /build install-pkgs -- qemu-user-static \
    gcc-${CC_TOOLCHAIN} g++-${CC_TOOLCHAIN} binutils-${CC_TOOLCHAIN}
COPY --from=rpi-debian --link . /build/rpi

FROM chan-quectel AS chan-quectel-cc-ext
ARG CC_TARGET
ARG CC_TOOLCHAIN

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    task -d /build install-pkgs-arch -- qemu-user-static \
    libsqlite3-dev:${CC_TARGET} libasound2-dev:${CC_TARGET}
COPY --from=arm-gnu-toolchain --link /build/arm-gnu-toolchain/${CC_TOOLCHAIN} /build/arm-gnu-toolchain/${CC_TOOLCHAIN}

FROM chan-quectel AS chan-quectel-rpi-ext
ARG CC_TARGET
ARG CC_TOOLCHAIN

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    task -d /build install-pkgs -- qemu-user-static
COPY --from=arm-gnu-toolchain --link /build/arm-gnu-toolchain/${CC_TOOLCHAIN} /build/arm-gnu-toolchain/${CC_TOOLCHAIN}
COPY --from=rpi-debian --link . /build/rpi
