# syntax=docker/dockerfile:1

ARG CMAKEVER=3.29.2
ARG ASTVER=18

FROM scratch AS dl-task
ADD --chmod=444 http://github.com/go-task/task/releases/latest/download/task_linux_amd64.rpm .

FROM centos:7.8.2003 as task-base
RUN --network=none mkdir /build
RUN --network=none --mount=type=bind,from=dl-task,target=/build/download yum install -y -q /build/download/task_linux_amd64.rpm
COPY --from=task --chown=nobody:nobody Taskfile.dist.yaml /build/Taskfile.yaml

FROM task-base as base

RUN --mount=type=cache,target=/var/cache/yum,sharing=locked task -d /build yum-install-pkgs -- rpm-build centos-release-scl scl-utils scl-utils-build file openssl sqlite-devel
RUN --mount=type=cache,target=/var/cache/yum,sharing=locked task -d /build yum-install-pkgs -- devtoolset-11
RUN --network=none task -d /build create-build-directory rpm-create-dotenv chown-build-directory -- nobody:nobody

FROM scratch as dl-cmake
ARG CMAKEVER
ADD --chmod=444 http://github.com/Kitware/CMake/releases/download/v${CMAKEVER}/cmake-${CMAKEVER}.tar.gz .

FROM base as cmake
ARG CMAKEVER

RUN --mount=type=cache,target=/var/cache/yum,sharing=locked task -d /build yum-install-pkgs -- openssl-devel 
USER nobody:nobody
RUN --network=none --mount=type=bind,from=dl-cmake,target=/build/download task -d /build cmake-extract
RUN --network=none scl enable devtoolset-11 'task -d /build cmake-bootstrap cmake-make cmake-install'

FROM scratch AS dl-asterisk
ARG ASTVER
ADD --chmod=444 http://downloads.asterisk.org/pub/telephony/asterisk/asterisk-${ASTVER}-current.tar.gz .

FROM base as asterisk
ARG ASTVER

RUN --mount=type=cache,target=/var/cache/yum,sharing=locked task -d /build yum-install-pkgs -- \
  patch wget bzip2 \
  openssl-devel libedit-devel libuuid-devel libxml2-devel alsa-lib-devel
USER nobody:nobody
RUN --network=none --mount=type=bind,from=dl-asterisk,target=/build/download task -d /build asterisk-extract-sources
RUN --mount=type=cache,target=/build/cache,sharing=locked,uid=99,gid=99 scl enable devtoolset-11 'task -d /build asterisk-configure-${ASTVER}'
RUN --network=none scl enable devtoolset-11 'task -d /build asterisk-build asterisk-install-headers'

FROM base as chan-quectel
LABEL description="Building chan_quectel on CentOS 7.8 which is a base OS for FreePBX."

RUN --mount=type=cache,target=/var/cache/yum,sharing=locked task -d /build yum-install-pkgs -- git alsa-lib-devel
ADD --chmod=755 http://github.com/jqlang/jq/releases/latest/download/jq-linux-amd64 /usr/bin/jq

COPY --from=cmake --link /build/install /
COPY --from=asterisk --link /build/install /
