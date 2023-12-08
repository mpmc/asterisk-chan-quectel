#!/bin/bash

echoerr() { echo "$@" 1>&2; }

if [ -z "$1" ]; then
    echoerr Serial device not specified
    exit 1
fi

TTYSERIAL=$1
if [ -h $TTYSERIAL ]; then
    TTYSERIAL=$(readlink $TTYSERIAL)
fi

if [ ! -c $TTYSERIAL ]; then
    echoerr $TTYSERIAL is not a character device
    exit 2
fi

shift
CMD="$@"

if [ -z "$CMD" ]; then
    echoerr Command not specified
    exit 3
fi

# IOCTL: Put the terminal into exclusive mode
TIOCEXCL=0x540c

socat -t0 -T4 \
    file:$TTYSERIAL,ioctl-void=$TIOCEXCL,flock-ex-nb,b115200,ignbrk=1,csize=3,cstopb=0,crtscts=0,clocal=1,cfmakeraw,crnl \
    exec:"$CMD",pipes,fdout=3
