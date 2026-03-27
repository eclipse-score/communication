#!/bin/sh

slogger2
waitfor /dev/slog

pci-server --config=/proc/boot/pci_server.cfg
waitfor /dev/pci

pipe
waitfor /dev/pipe

random -t -p
waitfor /dev/random

fsevmgr
waitfor /dev/fsnotify

devb-virtio

waitfor /dev/hd0
while ! mount -t dos -o case /dev/hd0 /shared; do
    echo "Failed to mount /shared. Retrying..."
done

waitfor /dev/hd1
while ! configure_persistency.sh; do
    echo "failed to configure persistency. retrying..."
done

waitfor /dev/hd2
while ! mount_ifs -f /dev/hd2 -m /opt; do
    echo "Failed to mount /opt. Retrying..."
done

devb-ram ram capacity=1 blk ramdisk=10m,cache=512k,vnode=256
waitfor /dev/ram0

mkqnx6fs -q /dev/ram0

mount -t qnx6 -o mntperms=777,noexec /dev/ram0 /tmp_discovery

io-pkt-v6-hc -d e1000 name=eth
waitfor /dev/socket
if_up -p -r 200 -m 10 eth0

ifconfig eth0 10.0.2.15 up
if_up -l -r 200 -m 10 eth0

mount -Tio-pkt /proc/boot/lsm-pf-v6.so
waitfor /dev/pf
waitfor /dev/bpf

mq
waitfor /dev/mq

mqueue
waitfor /dev/mqueue

devc-pty -n 32 &
pdebug 38080
