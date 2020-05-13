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

### Options

* WITH_UGID -- option enables uid/gid switching and specifying
  username/password when initializing mount directories.
* WITH_BLKID -- enables linking with libblkid, enables extra
  disk properties resolution capabilities for augmenting kernel
  events. If something does not work with kernel evens libblkid
  is likely to fix it.
* EVHEAD_MAGIC -- specifies unique magic for coupling diskmount
  and diskmountd to "ensure" custom local events integrity.

### Compile

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

Disk mount service automatically starts listening for NL (libudev or
kernel) events as well as custom events on local UNIX domain socket.
If libudev is not available it will start listening for kernel events.

### Using libudev events

Start diskmountd service will listen to udev events:

```
   diskmountd
```

### Using kernel uevents

If libudev is missing diskmountd will automatically fallback to kernel
uevent approach. However it is possible to force listening to kernel
events:

```
   diskmountd -k
```

### External events

This is totally optional and not too useful mechanism in standard
scenarios to trigger mount; all the events are captured via netlink.

Start diskmountd service:

```
   diskmountd
```

Setup udev rule to forward events:

```
   $ cat /etc/udev/rules.d/90-diskmount.rules
   ENV{DEVTYPE}=="partition", RUN+="/usr/bin/diskmount"
```

Or trigger manually:

```
   ACTION=add DEVNAME=/dev/sdb1 ID_FS_TYPE=ntfs /usr/bin/diskmount
```

## Monitoring

Printing disk mount config, mount tab and captured events:

```
   diskmountd -m
```
