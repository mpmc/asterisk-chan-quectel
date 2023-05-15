# Astersik channel driver for *Quectel* ~~and *SimCOM*~~ modules

See original [README](//github.com/IchthysMaranatha/asterisk-chan-quectel/blob/master/README.md) of this project.

----

This should work with *Quectel* modules such as EC20, EC21, EC25, EG9x ~~and *SimCOM* SIM7600~~ and possibly other models with *voice over USB* capability.
Tested with the **EC25-E** mini-PCIe module ~~and Waveshare SIM7600G-H dongle~~.
If the product page of your *Quectel* module contains the *Voice Over USB and UAC Application Note*, you should be good to go.

*SimCOM* support is **probably broken**. If you are using *SimCOM* module I strongly recomended using [original](//github.com/IchthysMaranatha/asterisk-chan-quectel) driver.
I do not have acces to *SimCOM* module thus cannot fix this driver for now.

# Changes

## Configuration

* `quectel_uac` option renamed to `uac` and it's a on/**off** switch now.
    
    `alsadev` option is also defaulted to `hw:Android`.

* New `multiparty` option (on/**off**).

    Ability to handle multiparty calls wastly complicates audio handling.
    Without multiparty calls audio handling is much simpler and uses less resources (CPU, memory, synchronization objects).
    I decided to turn off multiparty calls support by default.
    You can enable it but remember that multiparty calls were never working in UAC mode and even in TTY (serial) mode this support should be considered as **unstable**.
    When `mutliparty` is *off* all multiparty calls are **activley rejected**.

* `dtmf` option is a on/**off** switch now.

    DTMF detection is now performed by *Quectel* module itself (`AT+QTONEDET` command).

* New `msg_direct` option (**none**/on/off).

    Specify how to receive messages (`AT+CNMI` command):

    | value | description |
    | :---: | ------- |
    | **none** | do not change |
    | on | messages are received directly by `+CMT:` *URI*  |
    | off | messages are received by `AT+CMGR` command in response to `+CMTI` *URI* |

* New `msg_storage` option (**auto**/sm/me/mt/sr).

    Setting prefered message storage (`AT+CPMS` command):

    | value | description |
    | :---: | ------- |
    | **auto** | do not change |
    | sm | (U)SIM message storage |
    | *me* | mobile equipment message storage |
    | mt | same as *me* storage |
    | sr | SMS status report storage location |

* New `msg_service` option (**-1**/0/1).

    Selecting `Message Service` (`AT+CSMS` command):

    | value | description |
    | :---: | ------ |
    | **-1** | do not change |
    | 0 | SMS AT commands are compatible with *GSM phase 2* |
    | 1 | SMS AT commands are compatible with *GSM phase 2+* |

* New `moh` option (**on**/ott).

    Specify hold/unhold action:

    | value  | description |
    | :----: | ----------- |
    | **on** | play/stop MOH (previous default action) |
    | off    | disable/enable uplink voice using `AT+CMUT` command |

* `txgain` and `rxgain` options reimplented.

    * TX/RX gain is performed by module itself.
        
        This channel driver just sends `AT+QMIC`/`AT+QRXGAIN` (`AT+CTXVOL`/`AT+CRXVOL` for *SimCOM* module) commands.

    * New default value: **-1** - use current module setting, do not send *AT* commands at all.
    * Range: **0-65535**.

    See also `quectel autio gain tx` and `quectel audio gain rx` commands below.

* New `query_time` option (on/**off**).

    | value | description |
    | :---: | ----------- |
    | on | *ping* module with `AT+QLTS` (or `AT+CCLK` for *SimCOM* module) command |
    | **off** | *ping* module with standard `AT` command |

## Commands

* Additional fields in `show device status` command.

    Added `Access technology`, `Network Name`, `Short Network Name`, `Registered PLMN`, `Band` and `Module Time` fields:

    ```
        -------------- Status -------------
    Device                  : quectel0
    State                   : Free
    Audio UAC               : hw:Android
    Data                    : /dev/ttyUSB2
    Voice                   : Yes
    SMS                     : Yes
    Manufacturer            : Quectel
    Model                   : EC25
    Firmware                : EC25EXXXXXXXXXX
    IMEI                    : YYYYYYYYYYYYYYY
    IMSI                    : ZZZZZZZZZZZZZZZ
    GSM Registration Status : Registered, home network
    RSSI                    : 22, -69 dBm
    Access technology       : LTE
    Network Name            : Xxxxx
    Short Network Name      : Yxxxx
    Registered PLMN         : 26006
    Provider Name           : Zxxxxxx
    Band                    : LTE BAND 3
    Location area code      : 0000
    Cell ID                 : AAAAAAA
    Subscriber Number       : Unknown
    SMS Service Center      : +99000111222
    Module Time             : 2000/01/01,00:00:00+08,1
    Use UCS-2 encoding      : Yes
    Tasks in queue          : 0
    Commands in queue       : 0
    Call Waiting            : Disabled
    Current device state    : start
    Desired device state    : start
    When change state       : now
    Calls/Channels          : 0
        Active                : 0
        Held                  : 0
        Dialing               : 0
        Alerting              : 0
        Incoming              : 0
        Waiting               : 0
        Releasing             : 0
        Initializing          : 0
    ```

    Some of theese fields are constantly updated via `act` and `csq` notifications (see `AT+QINDCFG` command).

    Additional channel vaiables are also defined:
    
    - `QUECTELNETWORKNAME`
    - `QUECTELSHORTNETWORKNAME`
    - `QUECTELPROVIDER`
    - `QUECTELPLMN`
    - `QUECTELMCC`
    - `QUECTELMNC`

    `PLMN` (*Public Land Mobile Network Code*) combines `MCC` (*Mobile Country Code*) and `MNC` (*Mobile Network Code*).

    `SMS Service Center` is now decoded from *UCS-2*.

* Additional fields ins `quectel show device settings` command.

    Displaying new configuration options:

    ````
        ------------- Settings ------------
    Device                  : quectel0
    Audio UAC               : hw:Android
    Data                    : /dev/ttyUSB2
    IMEI                    : 
    IMSI                    : 
    Channel Language        : en
    Context                 : quectel-incoming
    Exten                   : 
    Group                   : 0
    RX gain                 : -1
    TX gain                 : -1
    Use CallingPres         : Yes
    Default CallingPres     : Presentation Allowed, Passed Screen
    Disable SMS             : No
    Message Service         : 1
    Message Storage         : ME
    Direct Message          : On
    Auto delete SMS         : Yes
    Reset Quectel           : No
    Call Waiting            : auto
    Multiparty Calls        : No
    DTMF Detection          : No
    Hold/Unhold Action      : Mute
    Query Time              : Yes
    Initial device state    : start
    ````

* New `quectel audio` commands:

    These are just wrappers around few audio-related *AT* commands:

    | command | *AT* command |
    | :------ | ------------ |
    | `quectel audio mode` | `AT+QAUDMOD` |
    | `quectel audio gain tx` | `AT+QMIC` |
    | `quectel audio gain rx` | `AT+QRXGAIN` |
    | `quectel audio loop` | `AT+QAUDLOOP` |

## Internal

* Hanging-up calls using `AT+QHUP` command with specific *release cause*.
* Handling calls is based on `ccinfo` notifications (see `AT+QINDCFG="ccinfo"` command) instead of `DSCI` (`AT^DSCI` command) call status notifications.
* Improved/extended *AT* commands response handler.

    Changes required to properly handle `AT+CMGL` command response.
    This command is now executed at initialization stage in order to receive unread messages.

* Simplyfying audio handling (when `mutliparty` is off, see above) in UAC and TTY mode.

    * Using less resources.
    * Much simpler error handling.
    * More debug messages.
    * Reorganized, improved and simplified code.

* Using `CMake` build system.

    See [Building](#building) section below.

* Improved debug messages.

    Non-printable characters are C escaped using `ast_escape_c` function. For example:

    ```
    DEBUG[11643]: at_read.c:93 at_read: [quectel0] [1][\r\n+QIND: \"csq\",27,99\r\n]
    DEBUG[11654]: at_read.c:93 at_read: [quectel0] [1][\r\n+CPMS: 0,25,0,25,0,25\r\n\r\nOK\r\n]
    DEBUG[11654]: at_read.c:93 at_read: [quectel0] [1][\r\n+QPCMV: 0,2\r\n\r\nOK\r\n]
    DEBUG[13411]: at_queue.c:181 at_write: [quectel0] [AT+QSPN;+QNWINFO\r]
    ```

* Many small optimizations.

# Building

As noted before [CMake](//cmake.org/) is now used as build system:


```
mkdir build
cmake -B build
cmake --build build
DESTDIR=$(pwd)/install cmake --install build
```

You may specify *Asterisk* version via `ASTERISK_VERSION_NUM` variable:

```
cmake .. -DASTERISK_VERSION_NUM=162000
```

## Generating Makefile for *OpenWRT* package

In order to generate `Makefile` for *OpenWRT* package one must install `openwrt` component:


```
DESTDIR=$(pwd)/install cmake --install build --component openwrt
```

## Building *Debian* package (*experimental*)

```
cmake --build build --target package
```

# Documentation

## Essential documents

* *EC2x&EC9x&EG2x-G&EM05 Series AT Commands Manual* (2021-02-24).
* *EC2x&EG9x Voice Over USB and UAC Application Note* (2019-02-18).
* *EC2x&EG2x&EG9x&EM05 Series QCFG AT Commands Manual* (2022-05-30).
* *UC20 AT Commands Manual* (2014-09-26).
* *SIM7500 SIM7600 Series AT Command Manual* (v3.0, 2021-11-18).
* *SIM7100 SIM7500 SIM7600 Series USB AUDIO Application Note* (v1.03, 2017-07-13).

## Resources

* [EC25 - Official downloads](//www.quectel.com/ProductDownload/EC25.html).
* [EC25 - Documentation from Sixfab](//sixfab.com/product/quectel-ec25-mini-pcie-4g-lte-module/).
* [EC25 - Documentation from Olimex](//github.com/OLIMEX/USB-gLINK/tree/master/DOCUMENTS).
* [Waveshare - SIM7600E-H_4G_HAT](//www.waveshare.com/wiki/SIM7600E-H_4G_HAT).
* [Waveshare - SIM7600G-H 4G HAT (B)](//www.waveshare.com/wiki/SIM7600G-H_4G_HAT_(B)).
* [Waveshare - SIM7600G-H 4G DTU](//www.waveshare.com/wiki/SIM7600G-H_4G_DTU).
* [Waveshare - SIM7600CE-T/CE-CNSE 4G Modules](//www.waveshare.com/wiki/SIM7600CE-T_4G_HAT).
* [Waveshare - SIM7600X 4G DONGLE](//www.waveshare.com/wiki/SIM7600CE-JT1S_4G_Dongle).

## Other links

* [Using EC20 module with asterisk and FreePBX to realize SMS forwarding and VoIP (Chinese)](//sparktour.me/2022/10/08/quectel-ec20-asterisk-freepbx-gsm-gateway/).
* [Asterisk Chan_Dongle (Blogspot)](//chan-dongle.blogspot.com/).
