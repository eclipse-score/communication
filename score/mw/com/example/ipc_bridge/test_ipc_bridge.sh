#!/bin/bash
set -eux

CPP_EXAMPLE_CMD="./bazel-bin/score/mw/com/example/ipc_bridge/ipc_bridge_cpp -n 20 -t 1000 -s score/mw/com/example/ipc_bridge/etc/mw_com_config.json"

RUST_EXAMPLE_CMD="./bazel-bin/score/mw/com/example/ipc_bridge/ipc_bridge_rs -s score/mw/com/example/ipc_bridge/etc/mw_com_config.json"

function build_examples() {
    bazel build \
        //score/mw/com/example/ipc_bridge:ipc_bridge_rs \
        //score/mw/com/example/ipc_bridge:ipc_bridge_cpp
}

function run_receiver_sender() {
    EXAMPLE_CMD_RECV="$1"
    EXAMPLE_CMD_SEND="$2"
    RECEIVER_RETURN_CODE=$3
    KILL_RECEIVER="$4"

    tempdir=$(mktemp -d /tmp/ipc_bridge.XXXXXX)

    # Run examples
    $EXAMPLE_CMD_RECV -m recv > "$tempdir/recv.log" 2>&1 &
    receiver_pid=$!
    $EXAMPLE_CMD_SEND -m send > "$tempdir/send.log" 2>&1

    # Check if the receiver received the message
    grep -q "Subscrib" "$tempdir/recv.log"
    grep -q "Received sample" "$tempdir/recv.log"
    rm -rf "$tempdir"

    # Kill receiver and check its return code
    if [[ "$KILL_RECEIVER" == "true" ]]; then
        kill $receiver_pid
    fi

    set +e
    wait $receiver_pid
    receiver_return_code=$?
    set -e
    [[ $RECEIVER_RETURN_CODE == $receiver_return_code ]]
}

build_examples

echo -e "\n\n\nRunning Rust receiver and Rust sender"
run_receiver_sender "$RUST_EXAMPLE_CMD" "$RUST_EXAMPLE_CMD" 143 true

echo -e "\n\n\nRunning C++ receiver and C++ sender"
run_receiver_sender "$CPP_EXAMPLE_CMD" "$CPP_EXAMPLE_CMD" 0 false

echo -e "\n\n\nRunning Rust receiver and C++ sender"
run_receiver_sender "$RUST_EXAMPLE_CMD" "$CPP_EXAMPLE_CMD" 143 true

echo -e "\n\n\nRunning C++ receiver and Rust sender"
run_receiver_sender "$CPP_EXAMPLE_CMD" "$RUST_EXAMPLE_CMD" 1 false
