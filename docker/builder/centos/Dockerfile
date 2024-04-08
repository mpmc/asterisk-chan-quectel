# syntax=docker/dockerfile:1

#
# Basic building pipeline
#

ARG BASEIMG
FROM ${BASEIMG}
ARG BRANCH=master
ARG ASTERISK_VERSION_NUM
ARG PRESET=rpm

USER nobody:nobody
RUN task -d /build chan-quectel-clone
RUN --network=none scl enable devtoolset-11 'task -d /build chan-quectel-preset'
RUN --network=none scl enable devtoolset-11 'task -d /build chan-quectel-package'
