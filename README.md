# Astersik channel driver for *Quectel* ~~and *SimCOM*~~ modules

See original [README](//github.com/IchthysMaranatha/asterisk-chan-quectel/blob/master/README.md) of this project.

This should work with *Quectel* modules such as EC20, EC21, EC25, EG9x ~~and *SimCOM* SIM7600~~ and possibly other models with *voice over USB* capability.
Tested with the **EC25-E** mini-PCIe module ~~and Waveshare SIM7600G-H dongle~~.
If the product page of your *Quectel* module contains the *Voice Over USB and UAC Application Note*, you should be good to go.

*SimCOM* support is **probably broken**. If you are using *SimCOM* module I stronly recomended using [original](//github.com/IchthysMaranatha/asterisk-chan-quectel) driver.
I do not have acces to *SimCOM* module thus cannot fix this driver for now.

## Changes

### Configuration changes

* `quectel_uac` option renamed to `uac` and it's a yes/**no** switch now. `alsadev` is also defaulted to `hw:Android`.
* New `multiparty` option (yes/**no**).

    Ability to handle multiparty calls wastly complicates audio handling.
    Without multiparty calls audio handling is much simpler and uses less resources (CPU, memory, synchronization objects).
    I decided to turn off multiparty calls support by default.
    You can enable it but remember that multiparty calls were never working on UAC mode and even in TTY (serial) mode this support should be considered as **unstable** and **untested**.
    When `mutliparty` is *off* all multiparty calls are **activley rejected**.

* `dtmf` option is a yes/**no** switch now.

    DTMF detection is now performed by *Quectel* module itself (`AT+QTONEDET` command).

### Commands changes

* Additional fields in `show device status` command.

    Added `Access technology`, `Network Name`, `Short Network Name`, `Registered PLMN` and `Band` fields:

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
    `SMS Service Center` is now decoded from *UCS-2*.

    Additional channel vaiables are also defined: `QUECTELNETWORKNAME`, `QUECTELSHORTNETWORKNAME`, `QUECTELPROVIDER`, `QUECTELPLMN`, `QUECTELMCC` and `QUECTELMNC`.[^1]

    [^1]: `PLMN` (*Public Land Mobile Network Code*) combines `MCC` (*Mobile Country Code*) and `MNC` (*Mobile Network Code*).

## Internal changes

* Hanging-up calls using `AT+QHUP` command with specific *release cause*.
* Handling calls is based on `ccinfo` notifications (see `AT+QINDCFG="ccinfo"` command) instead of `DSCI` (`AT^DSCI` command) call status notifications.
* Simplyfying audio handling (when `mutliparty` is off, see above) in UAC and TTY mode.

    - Using less resources.
    - Much simpler error handling.
    - More debug messages.
    - Reorganized, improved and simplified code.

* Using `CMake` build system. 

    Updated `Makefile` of *OpenWRT* package.

* Many small optimizations.

# Building:

As noted before *CMake* is now used as build system:


```
mkdir build
cd build
cmake ..
cd ..
cmake --build build
cmake --install build --prefix=<install location>
```

You may specify Asterisk version by `ASTERISK_VERSION_NUM` command:

```
cmake .. -DASTERISK_VERSION_NUM=162000
```

## Documentation

### Essential documents

* *EC2x&EC9x&EG2x-G&EM05 Series AT Commands Manual* (2021-02-24).
* *EC2x&EG9x Voice Over USB and UAC Application Note* (2019-02-18).
* *EC2x&EG2x&EG9x&EM05 Series QCFG AT Commands Manual* (2022-05-30).
* *UC20 AT Commands Manual* (2014-09-26).
* *SIM7500_SIM7600 Series AT Command Manual* (v3.0, 2021-11-18).
* *SIM7100 SIM7500 SIM7600 Series USB AUDIO Application Note* (v1.03, 2017-07-13).

### Resources

* [EC25 - Official downloads](//www.quectel.com/ProductDownload/EC25.html).
* [EC25 - Documentation from Sixfab](//sixfab.com/product/quectel-ec25-mini-pcie-4g-lte-module/).
* [EC25 - Documentation from Olimex](//github.com/OLIMEX/USB-gLINK/tree/master/DOCUMENTS).
* [Waveshare - SIM7600E-H_4G_HAT](//www.waveshare.com/wiki/SIM7600E-H_4G_HAT).
* [Waveshare - SIM7600G-H 4G HAT (B)](//www.waveshare.com/wiki/SIM7600G-H_4G_HAT_(B)).
* [Waveshare - SIM7600G-H 4G DTU](//www.waveshare.com/wiki/SIM7600G-H_4G_DTU).
* [Waveshare - SIM7600CE-T/CE-CNSE 4G Modules](//www.waveshare.com/wiki/SIM7600CE-T_4G_HAT).
* [Waveshare - SIM7600X 4G DONGLE](//www.waveshare.com/wiki/SIM7600CE-JT1S_4G_Dongle).

### Other links

* [Using EC20 module with asterisk and FreePBX to realize SMS forwarding and VoIP (Chinese)](//sparktour.me/2022/10/08/quectel-ec20-asterisk-freepbx-gsm-gateway/)
