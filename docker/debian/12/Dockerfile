# syntax=docker/dockerfile:1

#
# Building chan_quectel on Debian 12 (Bookworm)
#

ARG DEBIAN_FRONTEND=noninteractive
ARG CC_TARGET=armhf
ARG CC_TOOLCHAIN=arm-linux-gnueabihf
ARG ARM_GNU_TOOLCHAIN_VER=13.2.rel1
ARG ASTVER=20

FROM scratch AS dl-task
ADD --chmod=444 http://github.com/go-task/task/releases/latest/download/task_linux_amd64.deb .

FROM debian:bookworm-slim AS task-base
ARG DEBIAN_FRONTEND
RUN --network=none mkdir -p /build/task.d
RUN --network=none --mount=type=bind,from=dl-task,target=/build/download dpkg --install /build/download/task_linux_amd64.deb
COPY --from=task --chown=nobody:nogroup Taskfile.dist.yaml /build/Taskfile.yaml
COPY --from=task deb /build/task.d/deb

FROM scratch AS dl-arm-gnu-toolchain
ARG ARM_GNU_TOOLCHAIN_VER
ADD --chmod=444 http://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/${ARM_GNU_TOOLCHAIN_VER}/binrel/arm-gnu-toolchain-${ARM_GNU_TOOLCHAIN_VER}-x86_64-arm-none-linux-gnueabihf.tar.xz .
ADD --chmod=444 http://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/${ARM_GNU_TOOLCHAIN_VER}/binrel/arm-gnu-toolchain-${ARM_GNU_TOOLCHAIN_VER}-x86_64-aarch64-none-linux-gnu.tar.xz .

FROM task-base AS arm-gnu-toolchain
ARG ARM_GNU_TOOLCHAIN_VER

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    task -d /build/task.d/deb install-pkgs -- xz-utils
COPY --from=task arm-gnu-toolchain /build/task.d/arm-gnu-toolchain
RUN --network=none task -d /build/task.d/arm-gnu-toolchain create-directory

USER nobody:nogroup
RUN --network=none --mount=type=bind,from=dl-arm-gnu-toolchain,target=/build/download task -d /build/task.d/arm-gnu-toolchain prepare

FROM task-base AS base

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    task -d /build/task.d/deb install-pkgs -- file build-essential dpkg-dev ca-certificates jq libsqlite3-dev libssl3
RUN --network=none task -d /build create-build-directory && \
    task -d /build/task.d/deb create-dotenv && \
    task -d /build chown-build-directory -- nobody:nogroup

FROM scratch AS dl-asterisk
ARG ASTVER
ADD --chmod=444 http://downloads.asterisk.org/pub/telephony/asterisk/asterisk-${ASTVER}-current.tar.gz .

FROM base AS asterisk
ARG ASTVER

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    task -d /build/task.d/deb install-pkgs -- wget libssl-dev libedit-dev uuid-dev libxml2-dev
COPY --from=task asterisk /build/task.d/asterisk

USER nobody:nogroup
RUN --network=none --mount=type=bind,from=dl-asterisk,target=/build/download \
    task -d /build/task.d/asterisk extract-sources
RUN --mount=type=cache,target=/build/cache,sharing=locked,uid=65534,gid=65534 task -d /build/task.d/asterisk configure-${ASTVER}
RUN --network=none task -d /build/task.d/asterisk build install-headers

FROM scratch AS dl-ninja-build
ADD --chmod=444 https://api.github.com/repos/ninja-build/ninja/releases/latest ninja-build-latest.json

FROM base AS ninja-build
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    task -d /build/task.d/deb install-pkgs -- cmake curl libgtest-dev
COPY --from=task ninja-build /build/task.d/ninja-build

USER nobody:nogroup
RUN --mount=type=bind,from=dl-ninja-build,target=/build/cache task -d /build/task.d/ninja-build download2
RUN --network=none task -d /build/task.d/ninja-build configure build install

FROM base as chan-quectel
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    task -d /build/task.d/deb install-pkgs -- git cmake libasound2-dev
COPY --from=task chan-quectel /build/task.d/chan-quectel
COPY --from=asterisk --chown=root:root /build/install/ /
COPY --from=ninja-build --link --chown=root:root /build/install /

FROM chan-quectel AS chan-quectel-cc
ARG CC_TARGET
ARG CC_TOOLCHAIN

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    task -d /build/task.d/deb install-pkgs-arch -- qemu-user-static \
    gcc-${CC_TOOLCHAIN} g++-${CC_TOOLCHAIN} binutils-${CC_TOOLCHAIN} \
    libsqlite3-dev:${CC_TARGET} libasound2-dev:${CC_TARGET}

FROM --platform=linux/arm/v6 balenalib/rpi-debian:bookworm-build AS rpi-debian
ARG DEBIAN_FRONTEND
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
    task -d /build/task.d/deb install-pkgs -- qemu-user-static \
    gcc-${CC_TOOLCHAIN} g++-${CC_TOOLCHAIN} binutils-${CC_TOOLCHAIN}
COPY --from=task rpi /build/task.d/rpi
RUN --network=none --mount=type=bind,from=rpi-debian,target=/build/cache task -d /build/task.d/rpi sysroot

FROM chan-quectel AS chan-quectel-cc-ext
ARG CC_TARGET
ARG CC_TOOLCHAIN

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    task -d /build/task.d/deb install-pkgs-arch -- qemu-user-static \
    libsqlite3-dev:${CC_TARGET} libasound2-dev:${CC_TARGET}
COPY --from=arm-gnu-toolchain --link --chown=root:root /build/arm-gnu-toolchain/${CC_TOOLCHAIN} /build/arm-gnu-toolchain/${CC_TOOLCHAIN}

FROM chan-quectel AS chan-quectel-rpi-ext
ARG CC_TARGET
ARG CC_TOOLCHAIN

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    task -d /build/task.d/deb install-pkgs -- qemu-user-static
COPY --from=arm-gnu-toolchain --link --chown=root:root /build/arm-gnu-toolchain/${CC_TOOLCHAIN} /build/arm-gnu-toolchain/${CC_TOOLCHAIN}
COPY --from=task rpi /build/task.d/rpi
RUN --network=none --mount=type=bind,from=rpi-debian,target=/build/cache task -d /build/task.d/rpi sysroot
