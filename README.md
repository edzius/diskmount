# Diskmount

Project born out of despair and frustration.. Desperacy searching for
simple automatic disk mounter and frustration manually mounting and
unmounting disks. Due to above mentioned psychological condition and
for sake of simplicity utility is POSIX compatible without any cumber
auto-torture stuff.

## Sibling projects

There are some sibling projects providing similar functionality but
but non of them would simply mount devices into specified location
automatically without pain. Closest one to match needs was simple
usbmount, however i was urging for more deterministic and flexible
mounts.

https://github.com/rbrito/usbmount
https://github.com/storaged-project/udisks
https://git.kernel.org/pub/scm/linux/storage/autofs/autofs.git/

## Current approach

One way or another this approach is based on uevent kobject kernel
notifications. Current approach uses udev and it's hooks to read
events and pass them for mounting.

## Build

```
   make
```

Build outputs two binaries:

* diskmount -- client utility, proxies disk events.
* diskmountd -- daemon service, performs disk mounting.

## Configs

Configuration files are searched in few locations:

* local config -- ./diskmount.conf
* user config -- ~/.config/diskmount/config
* global config -- /etc/diskmount.conf

## Setup

Service uses configuration file to determine mount points and options:

```
   /dev/sdb1                  /mnt/disk-one
   DEV=/dev/sdb1              /tmp/read-only      -       ro
   LABEL=LaCie                /media/LaCie
   SERIAL=0EC125605221692E    /mnt/dummy          vfat
   UUID=2920-50BC             /mnt/my-drive       ntfs    rw,relatime
   PARTUUID=00014c47-01       /media/other
```

Structure is pretty much strait forward, similar to one used in fstab:
what to mount, where to mount, partition file system, mount options.

There are main keywords to identify source mount device:

* DEV -- by device file (no keyword also means device name).
* LABEL -- by disk label.
* SERIAL -- by disk serial number.
* UUID -- by FS UUID.
* PARTUUID -- by partition UUID.

## Running

Setup udev rule to forward events:

```
   $ cat /etc/udev/rules.d/90-diskmount.rules
   ENV{DEVTYPE}=="partition", RUN+="/usr/bin/diskmount"
```

Start diskmountd service:

```
   diskmountd
```

And that's it.

## Monitoring

Firguring out disk identification parameters:

```
   diskmountd -n
```
