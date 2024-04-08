# syntax=docker/dockerfile:1

#
# Basic building pipeline (CC)
#

ARG BASEIMG
FROM ${BASEIMG}
ARG BRANCH=master
ARG ASTERISK_VERSION_NUM
ARG PRESET
ARG CC_TARGET
ARG CC_TOOLCHAIN

USER nobody:nogroup
RUN task -d /build chan-quectel-clone
RUN --network=none task -d /build chan-quectel-preset
RUN --network=none task -d /build chan-quectel-package
