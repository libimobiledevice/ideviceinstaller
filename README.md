# ideviceinstaller

*A command-line application to manage apps and app archives on iOS devices.*

![](https://github.com/libimobiledevice/ideviceinstaller/actions/workflows/build.yml/badge.svg)

## Features

The ideviceinstaller application allows interacting with the app installation
service of an iOS device.

It makes use of the fabulous [libimobiledevice library](https://github.com/libimobiledevice/libimobiledevice) that allows
communication with iOS devices.

Some key features are:

- **Status:** Install, upgrade, uninstall, archive, restore and enumerate apps
- **Browse**: Allows to retrieve a list of installed apps with filter options
- **Install**: Supports app package, carrier bundle and developer .app directory
- **Format**: Allows command output in plist format
- **Compatibility**: Supports latest device firmware releases
- **Cross-Platform:** Tested on Linux, macOS, Windows and Android platforms

## Installation / Getting started

### Debian / Ubuntu Linux

First install all required dependencies and build tools:
```shell
sudo apt-get install \
	build-essential \
	checkinstall \
	git \
	autoconf \
	automake \
	libtool-bin \
	libplist-dev \
	libimobiledevice-dev \
	libzip-dev \
	usbmuxd
```

Continue with cloning the actual project repository:
```shell
git clone https://github.com/libimobiledevice/ideviceinstaller.git
cd ideviceinstaller
```

Now you can build and install it:
```shell
./autogen.sh
make
sudo make install
```

## Usage

First of all attach your device to your machine.

Then simply run:
```shell
ideviceinstaller --list-apps
```

This will print a list of `<appid>` identifiers (bundle identifiers) for use
with other commands (see further below).

To install an app from a package file use:
```shell
ideviceinstaller --install <file>
```

To uninstall an app with the `<appid>` from the device use:
```shell
ideviceinstaller --uninstall <appid>
```

Please consult the usage information or manual page for a full documentation of
available command line options:
```shell
ideviceinstaller --help
man ideviceinstaller
```

## Contributing

We welcome contributions from anyone and are grateful for every pull request!

If you'd like to contribute, please fork the `master` branch, change, commit and
send a pull request for review. Once approved it can be merged into the main
code base.

If you plan to contribute larger changes or a major refactoring, please create a
ticket first to discuss the idea upfront to ensure less effort for everyone.

Please make sure your contribution adheres to:
* Try to follow the code style of the project
* Commit messages should describe the change well without being too short
* Try to split larger changes into individual commits of a common domain
* Use your real name and a valid email address for your commits

We are still working on the guidelines so bear with us!

## Links

* Homepage: https://libimobiledevice.org/
* Repository: https://git.libimobiledevice.org/ideviceinstaller.git
* Repository (Mirror): https://github.com/libimobiledevice/ideviceinstaller.git
* Issue Tracker: https://github.com/libimobiledevice/ideviceinstaller/issues
* Mailing List: https://lists.libimobiledevice.org/mailman/listinfo/libimobiledevice-devel
* Twitter: https://twitter.com/libimobiledev

## License

This software is licensed under the [GNU General Public License v2.0](https://www.gnu.org/licenses/gpl-2.0.en.html),
also included in the repository in the `COPYING` file.

## Credits

Apple, iPhone, iPad, iPod, iPod Touch, Apple TV, Apple Watch, Mac, iOS,
iPadOS, tvOS, watchOS, and macOS are trademarks of Apple Inc.

ideviceinstaller is an independent software application and has not been
authorized, sponsored or otherwise approved by Apple Inc.

README Updated on: 2020-06-13
