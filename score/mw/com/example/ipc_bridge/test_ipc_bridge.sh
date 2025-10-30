#!/bin/bash
set -euxo pipefail

MANIFEST_LOCATION="/tmp/mw_com_lola_service_manifest.json"

# this cannot work if both sender and receiver expect 20 samples
# the sender has to send infinite samples and the receiver has to terminate after having received some
CPP_EXAMPLE_CMD="$(find * -name ipc_bridge_cpp) -s $MANIFEST_LOCATION --cycle-time 10"
RUST_EXAMPLE_CMD="$(find * -name ipc_bridge_rs) -s $MANIFEST_LOCATION --cycle-time 10"

function cleanup_lola() {
    # Ensure tests are run in a clean state

    # Linux
    rm -rf /dev/shm/lola-*6432*
    rm -rf /tmp/mw_com_lola/*/*6432*
    rm -rf /tmp/lola-*-*6432*_lock

    # QNX
    rm -rf /dev/shmem/lola-*6432*
    rm -rf /tmp_discovery/mw_com_lola/*/*6432*
    rm -rf /tmp_discovery/lola-*-*6432*_lock
}

function create_service_manifest() {
    local file="$1"
    local provider_user_id=$2
    local consumer_user_id=$3

    cat <<EOF > "$file"
{
  "serviceTypes": [
    {
      "serviceTypeName": "/bmw/adp/MapApiLanesStamped",
      "version": {
        "major": 1,
        "minor": 0
      },
      "bindings": [
        {
          "binding": "SHM",
          "serviceId": 6432,
          "events": [
            {
              "eventName": "map_api_lanes_stamped",
              "eventId": 1
            },
            {
              "eventName": "dummy_data_stamped",
              "eventId": 2
            }
          ]
        }
      ]
    }
  ],
  "serviceInstances": [
    {
      "instanceSpecifier": "xpad/cp60/MapApiLanesStamped",
      "serviceTypeName": "/bmw/adp/MapApiLanesStamped",
      "version": {
        "major": 1,
        "minor": 0
      },
      "instances": [
        {
          "instanceId": 1,
          "allowedConsumer": {
            "QM": [
              $consumer_user_id
            ]
          },
          "allowedProvider": {
            "QM": [
              $provider_user_id
            ]
          },
          "asil-level": "QM",
          "binding": "SHM",
          "events": [
            {
              "eventName": "map_api_lanes_stamped",
              "numberOfSampleSlots": 10,
              "maxSubscribers": 3
            }
          ]
        }
      ]
    }
  ]
}
EOF
}

function setup() {
    create_service_manifest "$MANIFEST_LOCATION" $(id -u) $(id -u)
    trap "rm -f $MANIFEST_LOCATION" EXIT
}

function run_receiver_sender() {
    EXAMPLE_CMD_RECV="$1"
    EXAMPLE_CMD_SEND="$2"

    tempdir=$(mktemp -d /tmp/ipc_bridge.XXXXXX)

    # Ensure we start with a clean state
    cleanup_lola

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
    cleanup_lola
}

setup

echo -e "\n\n\nRunning Rust receiver and Rust sender"
run_receiver_sender "$RUST_EXAMPLE_CMD" "$RUST_EXAMPLE_CMD"

echo -e "\n\n\nRunning C++ receiver and C++ sender"
run_receiver_sender "$CPP_EXAMPLE_CMD" "$CPP_EXAMPLE_CMD"

echo -e "\n\n\nRunning Rust receiver and C++ sender"
run_receiver_sender "$RUST_EXAMPLE_CMD" "$CPP_EXAMPLE_CMD"

echo -e "\n\n\nRunning C++ receiver and Rust sender"
run_receiver_sender "$CPP_EXAMPLE_CMD" "$RUST_EXAMPLE_CMD"
