# syntax=docker/dockerfile:1

FROM centos:7.8.2003 as base

# update and install required packages
RUN --mount=type=cache,target=/var/cache/yum,sharing=locked <<-EOF
#!/bin/bash -e

yum update -y -q
yum install -y -q centos-release-scl scl-utils scl-utils-build \
  file openssl sqlite-devel
yum install -y -q devtoolset-11
EOF

# make build directory
RUN --network=none <<-EOF
#!/bin/bash -e

mkdir -p /build/cache
mkdir -p /build/install
chown -R nobody:nobody /build
EOF

FROM base as cmake

# Full version of CMake
ARG CMAKEVER=3.29.0

RUN --mount=type=cache,target=/var/cache/yum,sharing=locked <<-EOF
#!/bin/bash -e

yum update -y -q
yum install -y -q openssl-devel
EOF

WORKDIR /build
USER nobody:nobody

ADD --chown=nobody:nobody \
  http://github.com/Kitware/CMake/releases/download/v${CMAKEVER}/cmake-${CMAKEVER}.tar.gz \
  .

RUN --network=none --mount=type=tmpfs,target=/build/cmake <<-EOF
#!/bin/bash -e

{
  echo "cd $(pwd)"
  echo 'tar -xf cmake-${CMAKEVER}.tar.gz' -C cmake
  echo 'cd cmake/cmake-${CMAKEVER}'
  echo './bootstrap --prefix=/usr'
  echo 'make -j$(nproc)'
  echo 'DESTDIR=/build/install make install'
} | scl enable devtoolset-11 -
EOF

FROM base as asterisk

# Only major version of Asterisk
ARG ASTVER=18

RUN --mount=type=cache,target=/var/cache/yum,sharing=locked <<-EOF
#!/bin/bash -e

yum update -y -q
yum install -y -q \
  patch wget bzip2 \
  openssl-devel libedit-devel libuuid-devel libxml2-devel alsa-lib-devel
EOF

WORKDIR /build
USER nobody:nobody

ADD --chown=nobody:nobody \
  http://downloads.asterisk.org/pub/telephony/asterisk/asterisk-${ASTVER}-current.tar.gz \
  .

RUN --network=none <<EOF
#!/bin/bash -e

tar -xf asterisk-${ASTVER}-current.tar.gz
ast=$(cd asterisk-${ASTVER}.*; pwd)
ln -sr ${ast} asterisk-${ASTVER}
EOF

WORKDIR /build/asterisk-${ASTVER}
COPY --chown=nobody:nobody configure-asterisk.sh .
RUN --mount=type=cache,target=/build/cache,sharing=locked,uid=99,gid=99 <<-EOF
#!/bin/bash -e

{
  echo "cd $(pwd)"
  echo './configure-asterisk.sh'
} | scl enable devtoolset-11 -
EOF

RUN --network=none <<-EOF
#!/bin/bash -e

{
  echo "cd $(pwd)"
  echo 'make -j$(nproc) OPTIMIZE=-fmerge-constants'
  echo 'DESTDIR=/build/install make install-headers'
} | scl enable devtoolset-11 -
EOF

FROM base as chan-quectel-base
LABEL description="Building chan_quectel on CentOS 7.8 which is a base OS for FreePBX."

# git branch
ARG BRANCH=master

RUN --mount=type=cache,target=/var/cache/yum,sharing=locked <<-EOF
#!/bin/bash -e

yum update -y -q
yum install -y -q rpm-build git alsa-lib-devel
EOF

COPY --from=cmake --link /build/install /
COPY --from=asterisk --link /build/install /

ADD --chmod=755 http://github.com/jqlang/jq/releases/download/jq-1.7.1/jq-linux-amd64 /usr/bin/jq

WORKDIR /build
USER nobody:nobody

ADD "http://www.random.org/cgi-bin/randbyte?nbytes=10&format=h" skipcache
RUN git clone -q --branch=${BRANCH} http://github.com/RoEdAl/asterisk-chan-quectel.git chan-quectel

USER root

FROM chan-quectel-base as chan-quectel

USER nobody:nobody
WORKDIR /build/chan-quectel

RUN --network=none --mount=type=tmpfs,target=/build/chan-quectel/build <<-EOF
#!/bin/bash -e

{
  echo "cd $(pwd)"
  echo './get-build-flags.sh rpm > CMakeUserPresets.json'
  echo 'cmake -DASTERISK_VERSION_NUM=180000 --preset rpm'
  echo 'cmake --build --preset package-rpm'
} | scl enable devtoolset-11 -

mkdir package-tgz
mv package/*.tar.gz package-tgz
mv package/*.tar.gz.sha256 package-tgz
EOF

USER root
