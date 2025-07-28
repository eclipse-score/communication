#!/bin/bash

set -xu

if [ -z "$1" ]; then
    echo "Usage: $0 <basefolder>"
    exit 1
fi
BASEFOLDER=$1
IMAGE="${BASEFOLDER}/fastdev-ubuntu-ebcl-deb-qemuarm64.wic"
KERNEL="${BASEFOLDER}/fastdev-ubuntu-ebcl-deb-qemuarm64-vmlinux"
if [ ! -d "${BASEFOLDER}" ] || [ ! -f "${IMAGE}" ] || [ ! -f "${KERNEL}" ] ; then
    echo "Run \"bazel build --config=aarch64 --spawn_strategy=local //score/mw/com/example/ipc_bridge_hi_wrapper:fastdev-image\" first to fetch the image"
fi

MACHINE="virt,virtualization=true,gic-version=3"
CPU="cortex-a53"
SMP="8"
MEM="4G"
KERNEL_ARGS=("-append" "root=/dev/vda1 sdk_enable lisa_syscall_whitelist=2026 rw sharedmem.enable_sharedmem=0 init=/usr/bin/ebclfsa-cflinit")
DISK_ARGS="-device virtio-blk-device,drive=vd0 -drive if=none,format=raw,file=${IMAGE},id=vd0"
NETWORK_ARGS="-netdev user,id=net0,net=192.168.7.0/24,dhcpstart=192.168.7.2,dns=192.168.7.3,host=192.168.7.5,hostfwd=tcp::2222-:22,hostfwd=tcp::3333-:3333 -device virtio-net-device,netdev=net0 "

if ! command -v qemu-system-aarch64 > /dev/null; then
    echo "Please install qemu-system-aarch64"
    exit 1
fi

chmod +w ${IMAGE}

exec qemu-system-aarch64 -m "${MEM}" -machine "${MACHINE}" -cpu "${CPU}" \
    -smp "${SMP}" -kernel "${KERNEL}" "${KERNEL_ARGS[@]}" ${DISK_ARGS} \
    ${NETWORK_ARGS} -nographic ${@:2}
