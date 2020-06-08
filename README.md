# ideviceinstaller

## About

The ideviceinstaller tool allows interacting with the installation_proxy service
of an iOS device allowing to install, upgrade, uninstall, archive, restore
and enumerate installed or archived apps.

It makes use of the fabulous libimobiledevice library that allows communication
with iOS devices.

## Requirements

Development Packages of:
* libimobiledevice
* libplist
* libzip

## Installation

To compile run:
```bash
./autogen.sh
make
sudo make install
```

## Who/What/Where?

* Home: https://libimobiledevice.org/
* Code: `git clone https://git.libimobiledevice.org/ideviceinstaller.git`
* Code (Mirror): `git clone https://github.com/libimobiledevice/ideviceinstaller.git`
* Tickets: https://github.com/libimobiledevice/ideviceinstaller/issues
* Mailing List: https://lists.libimobiledevice.org/mailman/listinfo/libimobiledevice-devel
* Twitter: https://twitter.com/libimobiledev

## Credits

Apple, iPhone, iPod, iPad, Apple TV and iPod Touch are trademarks of Apple Inc.

ideviceinstaller is an independent software application and has not been
authorized, sponsored or otherwise approved by Apple Inc.

README Updated on: 2020-06-08
