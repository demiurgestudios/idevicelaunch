# idevicelaunch
idevicelaunch utility, built on libimobiledevice. Variation of idevicedebug.

## Overview
Launch an app with its bundle id. In contrast to
[idevicedebug](https://github.com/libimobiledevice/libimobiledevice/blob/master/tools/idevicedebug.c),
detaches and exits after launch.

## Usage
`idevicelaunch start BUNDLEID`

* `start`: launches the app with given bundle id. Must be installed on the device.
