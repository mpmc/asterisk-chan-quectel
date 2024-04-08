# syntax=docker/dockerfile:1

#
# Basic building pipeline
#

ARG BASEIMG
FROM ${BASEIMG}
ARG BRANCH=master
ARG ASTERISK_VERSION_NUM
ARG PRESET

USER nobody:nogroup
RUN task -d /build chan-quectel-clone
RUN --network=none task -d /build chan-quectel-preset
RUN --network=none task -d /build chan-quectel-package
