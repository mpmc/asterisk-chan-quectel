# *udev* rules for creating IMEI- and IMSI-based symlinks of serial devices

This is an example of *udev* rules creating IMEI- and IMSI-based symlinks of serial devices.
Symlinks are created in `/dev/modem/by-imei` and `/dev/modem/by-imsi` directories.

* **IMEI** - **I**nternational **M**obile **E**quipment **I**dentity.
* **IMSI** - **I**nternational **M**obile **S**ubscriber **I**dentity.

## Requirements

1. [`socat`](http://www.dest-unreach.org/socat/).

## Installation

1. Copy [`65-modem.rules`](65-modem.rules) file to `/etc/udev/rules.d` directory.
2. Copy [`read-modem-properties.sh`](read-modem-properties.sh) file to `/usr/lib/udev` directory.
3. Reload *udev* rules:

   ```sh
   sudo udevadm control --reload
   sudo udevadm trigger
   ```

## AT Commands

1. `AT+CGSN` - Request product serial number identification.
1. `AT+CIMI` - Request international mobile subscriber identity.

## Links

* [`socat` manual page](http://www.dest-unreach.org/socat/doc/socat.html).
* [Arch Linux - *udev*](https://wiki.archlinux.org/title/udev).
