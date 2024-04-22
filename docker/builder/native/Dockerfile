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
RUN task -d /build/task.d/chan-quectel clone
RUN --network=none task -d /build/task.d/chan-quectel configure
RUN --network=none task -d /build/task.d/chan-quectel build
