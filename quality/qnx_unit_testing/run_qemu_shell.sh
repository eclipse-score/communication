#!/bin/bash

set -euo pipefail

IFS_IMAGE=$1
PERSISTENT_IMAGE=$2
TEST_RESULT_IMAGE=$3
TEST_IMAGE=$4

writable_img=$(mktemp)
cp "${TEST_RESULT_IMAGE}" "${writable_img}"

writable_img_persistent=$(mktemp)
cp "${PERSISTENT_IMAGE}" "${writable_img_persistent}"

DEBUG_PORT=""
if [ $# -eq 6 ]; then
    if [ "$5" == "--debug-port" ]; then
        DEBUG_PORT="$6"
    else
        echo "ERROR: Unknown argument '$5'"
        exit 1
    fi
fi

NETWORK=""
if [ ! -z "${DEBUG_PORT}" ]; then
    echo "WARNING: Debugging enabled on port ${DEBUG_PORT}"
    NETWORK="-netdev user,id=net0,hostfwd=tcp:127.0.0.1:${DEBUG_PORT}-10.0.2.15:38080 -device e1000,netdev=net0"
fi

ACCEL="-cpu qemu64,+cx16,+lahf-lm,+popcnt,+pni,+sse4.1,+sse4.2,+ssse3,+avx,+avx2,+bmi1,+bmi2,+f16c,+fma,+movbe"

DISABLE_KVM="${DISABLE_KVM:-0}"

if [[ -e /dev/kvm && -r /dev/kvm ]]; then
    echo "KVM supported!"

    if [[ "${DISABLE_KVM}" == 0 ]]; then
        ACCEL="-enable-kvm -cpu host,-xsave"
    else
        echo "KVM explicitly disabled!"
    fi
fi

qemu-system-x86_64 \
                -smp 2 \
                -m 2G \
                ${ACCEL} \
                -nographic \
                -kernel "${IFS_IMAGE}" \
                -serial mon:stdio \
                -object rng-random,filename=/dev/urandom,id=rng0 \
                -device virtio-rng-pci,rng=rng0 \
                ${NETWORK} \
                -drive file="${writable_img}",if=virtio,format=raw,index=0,media=disk \
                -drive file="${writable_img_persistent}",if=virtio,format=raw,index=1,media=disk \
                -drive file="${TEST_IMAGE}",if=virtio,format=raw,index=2,media=disk,readonly=on
