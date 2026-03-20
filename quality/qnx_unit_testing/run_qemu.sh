#!/bin/bash

set -euo pipefail

IFS_IMAGE=$1
PERSISTENT_IMAGE=$2
PROCESS_TESTS_RESULTS=$3
TEST_RESULT_IMAGE=$4
TEST_IMAGE=$5

writable_img=$(mktemp)
cp "${TEST_RESULT_IMAGE}" "${writable_img}"

writable_img_persistent=$(mktemp)
cp "${PERSISTENT_IMAGE}" "${writable_img_persistent}"

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
                -no-reboot \
                -object rng-random,filename=/dev/urandom,id=rng0 \
                -device virtio-rng-pci,rng=rng0 \
                -drive file="${writable_img}",if=virtio,format=raw,index=0,media=disk \
                -drive file="${writable_img_persistent}",if=virtio,format=raw,index=1,media=disk \
                -drive file="${TEST_IMAGE}",if=virtio,format=raw,index=2,media=disk,readonly=on \
                2>&1 | sed 's/[^[:print:]]//g' | sed 's/\r//'

${PROCESS_TESTS_RESULTS} "${writable_img}"
return_code=$?

rm -rf "${writable_img}"
rm -rf "${writable_img_persistent}"

exit "${return_code}"
