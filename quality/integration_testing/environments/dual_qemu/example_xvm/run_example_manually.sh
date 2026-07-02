#!/usr/bin/env bash
# *******************************************************************************
# Copyright (c) 2026 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************
#
# Launch TWO QNX QEMU VMs that share a single ivshmem region and run the CROSS-VM
# PRODUCTION lib-memory example (score::memory::shared over the ivshmem BAR) by hand,
# so you can SSH in and drive the producer/consumer roles yourself.
#
# Unlike the hand-rolled offset_list demo in ../example, this maps the BAR through the
# real SharedMemoryFactory: one VM Create()s + inits an OffsetPtr list, the other Open()s
# it and resolves the graph. They rendezvous via a handshake header in the last page of
# the BAR, so start BOTH roughly together (each waits for the other).
#
# Usage:
#   1. Build the IFS image (contains lib_memory_ivshmem_xvm + sshd):
#        bazel build --config=qnx_x86_64 \
#          //quality/integration_testing/environments/dual_qemu/example_xvm:_init_ifs_test_lib_memory_ivshmem_xvm
#   2. Start both VMs (from the repo root):
#        ./quality/integration_testing/environments/dual_qemu/example_xvm/run_example_manually.sh
#   3. In two other terminals, SSH in (password is empty):
#        ssh -p 2222 root@localhost      # VM-A (producer)
#        ssh -p 2223 root@localhost      # VM-B (consumer)
#   4. Run the example (the app reaches the BAR through pci-server, so no /dev/ivshmem0
#      guest driver is needed). Start BOTH roughly together:
#        # on VM-A (producer): Create()s + inits the list on the BAR
#        /opt/lib_memory_ivshmem_xvm/bin/lib_memory_ivshmem_xvm --role producer
#        # on VM-B (consumer): Open()s the same BAR and verifies the OffsetPtr list
#        /opt/lib_memory_ivshmem_xvm/bin/lib_memory_ivshmem_xvm --role consumer
#      Both print "verified" on success.
#   5. Stop the VMs:
#        ./quality/integration_testing/environments/dual_qemu/example_xvm/run_example_manually.sh stop
#
set -euo pipefail

QEMU=${QEMU:-/usr/bin/qemu-system-x86_64}
IMG=${IMG:-bazel-bin/quality/integration_testing/environments/dual_qemu/example_xvm/init_ifs_test_lib_memory_ivshmem_xvm}
SHM_PATH=${SHM_PATH:-/dev/shm/score_ivshmem}
SHM_SIZE=${SHM_SIZE:-4M}
RAM=${RAM:-1G}
# Single core on purpose: booting two QNX guests concurrently under KVM can wedge the
# second guest while it brings up a secondary CPU (SMP AP startup) under the -kernel
# boot path. One core per VM avoids that race; the example needs no extra cores.
CORES=${CORES:-1}
# Leave CPU empty by default: it is chosen based on the accelerator below (host for KVM,
# max for TCG) to avoid "host doesn't support requested feature" warnings. Override by
# exporting CPU=<model>.
CPU=${CPU:-}
SSH_PORT_A=${SSH_PORT_A:-2222}
SSH_PORT_B=${SSH_PORT_B:-2223}
RUN_DIR=${RUN_DIR:-/tmp/dual_qemu}

stop_vms() {
    for pidfile in "${RUN_DIR}/vm_a.pid" "${RUN_DIR}/vm_b.pid"; do
        if [[ -f "${pidfile}" ]]; then
            pid=$(cat "${pidfile}")
            if kill -0 "${pid}" 2>/dev/null; then
                echo "Stopping QEMU pid ${pid}"
                kill "${pid}" 2>/dev/null || true
            fi
            rm -f "${pidfile}"
        fi
    done
}

if [[ "${1:-start}" == "stop" ]]; then
    stop_vms
    exit 0
fi

if [[ ! -f "${IMG}" ]]; then
    echo "IFS image not found: ${IMG}" >&2
    echo "Build it first with:" >&2
    echo "  bazel build --config=qnx_x86_64 //quality/integration_testing/environments/dual_qemu/example_xvm:_init_ifs_test_lib_memory_ivshmem_xvm" >&2
    exit 1
fi

# Clean up any VMs we previously started, so re-running does not orphan the old QEMU
# processes (which would keep holding the SSH ports).
stop_vms

# Fail early with a clear message if the SSH ports are still taken by something else.
for port in "${SSH_PORT_A}" "${SSH_PORT_B}"; do
    if ss -ltn 2>/dev/null | grep -q ":${port} "; then
        echo "Host port ${port} is already in use." >&2
        echo "  Find the holder: ss -ltnp | grep :${port}" >&2
        echo "  Then kill it, or pick other ports: SSH_PORT_A=3000 SSH_PORT_B=3001 $0" >&2
        exit 1
    fi
done

# Use hardware virtualization when available, otherwise fall back to TCG.
# Pick a CPU model that matches the accelerator so QEMU does not warn about CPU features
# the host lacks (e.g. AVX-512 on a non-AVX-512 host).
if [[ -r /dev/kvm ]]; then
    ACCEL=(--enable-kvm)
    CPU=${CPU:-host}
else
    echo "No readable /dev/kvm, falling back to TCG (slower)."
    ACCEL=(-accel tcg)
    CPU=${CPU:-max}
fi

mkdir -p "${RUN_DIR}"

# One shared backing file mapped by BOTH VMs => the shared memory region.
echo "Creating shared ivshmem backing file ${SHM_PATH} (${SHM_SIZE})"
truncate -s "${SHM_SIZE}" "${SHM_PATH}"

# Shared ivshmem device flags (identical for both VMs, pointing at the same file).
IVSHMEM_ARGS=(
    -object "memory-backend-file,id=ivshmem_mem,mem-path=${SHM_PATH},size=${SHM_SIZE},share=on"
    -device "ivshmem-plain,memdev=ivshmem_mem"
)

start_vm() {
    local name=$1 ssh_port=$2 serial_log=$3 pidfile=$4 mac=$5
    echo "Starting ${name}: ssh port ${ssh_port}, serial log ${serial_log}"
    "${QEMU}" \
        "${ACCEL[@]}" \
        -smp "${CORES},maxcpus=${CORES},cores=${CORES}" \
        -cpu "${CPU}" \
        -m "${RAM}" \
        -kernel "${IMG}" \
        -display none \
        -serial "file:${serial_log}" \
        -object rng-random,filename=/dev/urandom,id=rng0 \
        -device virtio-rng-pci,rng=rng0 \
        "${IVSHMEM_ARGS[@]}" \
        -netdev "user,id=net1,hostfwd=tcp::${ssh_port}-:22" \
        -device virtio-net-pci,netdev=net1,mac=${mac} \
        &
    echo $! > "${pidfile}"
}

# Distinct MACs: two VMs sharing QEMU's default MAC makes the 2nd VM's sshd hang.
start_vm "VM-A" "${SSH_PORT_A}" "${RUN_DIR}/vm_a_serial.log" "${RUN_DIR}/vm_a.pid" "52:54:00:00:00:01"
start_vm "VM-B" "${SSH_PORT_B}" "${RUN_DIR}/vm_b_serial.log" "${RUN_DIR}/vm_b.pid" "52:54:00:00:01:01"

cat <<EOF

Both VMs are booting. Once up (watch ${RUN_DIR}/vm_*_serial.log), connect with:
  ssh -p ${SSH_PORT_A} root@localhost      # VM-A (producer)
  ssh -p ${SSH_PORT_B} root@localhost      # VM-B (consumer)

Then run the cross-VM lib-memory example (start both roughly together, each waits for
the other via the in-BAR handshake; both print "verified" on success):
  # on VM-A (producer): Create()s + inits the OffsetPtr list on the BAR
  /opt/lib_memory_ivshmem_xvm/bin/lib_memory_ivshmem_xvm --role producer
  # on VM-B (consumer): Open()s the same BAR and resolves + verifies the list
  /opt/lib_memory_ivshmem_xvm/bin/lib_memory_ivshmem_xvm --role consumer

Stop both VMs with:
  $0 stop
EOF
