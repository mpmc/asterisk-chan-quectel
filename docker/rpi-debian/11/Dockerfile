# syntax=docker/dockerfile:1

#
# Building chan_quectel on Raspbian 11 (Bullseye)
#
ARG DEBIAN_FRONTEND=noninteractive
ARG CMAKEVER=3.29.2

FROM scratch AS dl-task
ADD --chmod=444 http://github.com/go-task/task/releases/latest/download/task_linux_arm.deb .

FROM balenalib/rpi-debian:bullseye-build AS task-base
ARG DEBIAN_FRONTEND
RUN --network=none mkdir /build
RUN --network=none --mount=type=bind,from=dl-task,target=/build/download dpkg --install /build/download/task_linux_arm.deb
COPY --from=task --chown=nobody:nogroup Taskfile.dist.yaml /build/Taskfile.yaml

FROM task-base AS base
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked -- \
    task -d /build install-pkgs -- file ca-certificates jq dpkg-dev libsqlite3-dev libssl1.1
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

FROM base as chan-quectel
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    task -d /build install-pkgs -- libasound2-dev asterisk-dev
COPY --from=cmake --link /build/install/ /
