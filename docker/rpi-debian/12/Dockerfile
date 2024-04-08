# syntax=docker/dockerfile:1

#
# Building chan_quectel on RaspberryPiOS 12 (Bookworm)
#
ARG DEBIAN_FRONTEND=noninteractive

FROM scratch AS dl-task
ADD --chmod=444 http://github.com/go-task/task/releases/latest/download/task_linux_arm.deb .

FROM balenalib/rpi-debian:bookworm-build AS task-base
ARG DEBIAN_FRONTEND
RUN --network=none mkdir /build
RUN --network=none --mount=type=bind,from=dl-task,target=/build/download dpkg --install /build/download/task_linux_arm.deb
COPY --from=task --chown=nobody:nogroup Taskfile.dist.yaml /build/Taskfile.yaml

FROM task-base AS base
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked -- \
    task -d /build install-pkgs -- file ca-certificates jq dpkg-dev libsqlite3-dev libssl3
RUN --network=none task -d /build create-build-directory create-dotenv chown-build-directory -- nobody:nogroup

FROM base AS chan-quectel
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    task -d /build install-pkgs -- cmake libasound2-dev asterisk-dev
