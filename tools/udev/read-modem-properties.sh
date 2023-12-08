#!/bin/bash

#
# Additional file descriptors:
#
# 3 : TTY out
#
# AT commands:
#
# AT+CGSN : Request product serial number identification
# AT+CIMI : Request international mobile subscriber identity
#

echoerr() { echo "$@" 1>&2; }
echotty() { echo "$@" 1>&3; }

resp_15_digits() {
    local ATOUT
    local ATRES

    while true; do
        if ! read -s ATOUT ATRES; then
            break
        fi

        if [ -z "$ATOUT" ]; then
            continue
        fi

        case $ATOUT in
            OK) return 0;;
            +CME|ERROR) return 1;;
            [0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9])
                echo $1=$ATOUT;;
            *) echoerr AT RESPONSE: $ATOUT $ATRES;;
        esac
    done
    return 1
}

echotty 'AT+CGSN' && resp_15_digits IMEI
echotty 'AT+CIMI' && resp_15_digits IMSI
