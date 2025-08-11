#!/bin/bash
set -eux

# this cannot work if both sender and receiver expect 20 samples
# the sender has to send infinite samples and the receiver has to terminate after having received some
CPP_EXAMPLE_CMD="score/mw/com/example/ipc_bridge/ipc_bridge_cpp -s score/mw/com/example/ipc_bridge/etc/mw_com_config.json --cycle-time 1000"
RUST_EXAMPLE_CMD="score/mw/com/example/ipc_bridge/ipc_bridge_rs -s score/mw/com/example/ipc_bridge/etc/mw_com_config.json --cycle-time 1000"

function run_receiver_sender() {
    EXAMPLE_CMD_RECV="$1"
    EXAMPLE_CMD_SEND="$2"

    tempdir=$(mktemp -d /tmp/ipc_bridge.XXXXXX)

    # Ensure we start with a clean state
    rm -rf /tmp/mw_com_lola
    rm -rf /dev/shm/lola-*

    # Run examples
    $EXAMPLE_CMD_SEND --num-cycles 0 --mode send > "$tempdir/send.log" 2>&1 &
    sender_pid=$!
    $EXAMPLE_CMD_RECV --num-cycles 2 --mode recv > "$tempdir/recv.log" 2>&1

    # Check if the receiver received the message
    grep -q "Subscrib" "$tempdir/recv.log"
    grep -q "Received sample" "$tempdir/recv.log"
    rm -rf "$tempdir"

    # Kill sender and check its return code
    kill $sender_pid

    set +e
    wait $sender_pid
    sender_return_code=$?
    set -e
    [[ "143" == "$sender_return_code" ]]

    # Cleanup due to SIGINT
    rm -rf /tmp/mw_com_lola
    rm -rf /dev/shm/lola-*
}

echo -e "\n\n\nRunning Rust receiver and Rust sender"
run_receiver_sender "$RUST_EXAMPLE_CMD" "$RUST_EXAMPLE_CMD"

echo -e "\n\n\nRunning C++ receiver and C++ sender"
run_receiver_sender "$CPP_EXAMPLE_CMD" "$CPP_EXAMPLE_CMD"

echo -e "\n\n\nRunning Rust receiver and C++ sender"
run_receiver_sender "$RUST_EXAMPLE_CMD" "$CPP_EXAMPLE_CMD"

echo -e "\n\n\nRunning C++ receiver and Rust sender"
run_receiver_sender "$CPP_EXAMPLE_CMD" "$RUST_EXAMPLE_CMD"
