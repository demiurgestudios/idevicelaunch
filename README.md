# idevicelaunch
idevicelaunch utility, built on libimobiledevice. Variation of idevicedebug.

## Overview
Command start and stop semantics. In contrast to [idevicedebug](https://github.com/libimobiledevice/libimobiledevice/blob/master/tools/idevicedebug.c), which launches, remains attached, and then kills the app in a single execution.

## Usage
`idevicelaunch [start|stop] BUNDLEID`

* `start`: launches the app with given bundle id. Must be installed on the device.
* `stop`: kills the app with given bundle id, if running.
