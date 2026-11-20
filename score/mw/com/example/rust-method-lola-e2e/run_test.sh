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
# Runs every cross-process scenario of rust_method_lola_e2e and prints a summary.
#
#   run_test.sh <binary> <config template> [scenario ...]
#
# Without scenarios it runs all of them. Exit code 0 only if every scenario that ran passed.
set -u
BIN="$(realpath "$1")"
TEMPLATE="$(realpath "$2")"
shift 2
SCENARIOS=("$@")
[ ${#SCENARIOS[@]} -eq 0 ] && SCENARIOS=(basic seq concurrent overflow queue two_consumers early crash handler_panic dup_appid dup_appid_producer)

WORK="$(mktemp -d)"
chmod 0755 "$WORK"
# mw::com identifies a process by its configured applicationID (shared-memory and message-passing
# names derive from it), so every process gets its own: cfg <applicationID>.
cfg() {
    local out="$WORK/$1.json"
    sed -e "s|@APPID@|$1|" "$TEMPLATE" > "$out"
    echo "$out"
}
PRODUCER_APP=6501

PRODUCER=""
start_producer() {
    "$BIN" producer "$(cfg $PRODUCER_APP)" > "$WORK/producer.log" 2>&1 &
    PRODUCER=$!
    for _ in $(seq 1 100); do grep -q "PRODUCER offered" "$WORK/producer.log" 2>/dev/null && return 0; sleep 0.1; done
    echo "producer did not offer:"; cat "$WORK/producer.log"; return 1
}
stop_producer() {
    [ -n "$PRODUCER" ] && kill "$PRODUCER" 2>/dev/null && wait "$PRODUCER" 2>/dev/null
    PRODUCER=""
}
trap 'stop_producer; jobs -p | xargs -r kill 2>/dev/null; rm -rf "$WORK"' EXIT

consumer() { timeout 120 "$BIN" consumer "$(cfg "$1")" "$2"; }

declare -A RESULT
run() {
    local name="$1"; shift
    echo; echo "=================== $name"
    "$@"; local rc=$?
    case $rc in 0) RESULT[$name]=PASS ;; 2) RESULT[$name]=SKIP ;; *) RESULT[$name]=FAIL ;; esac
    echo "--- producer log ($name)"; tail -n 20 "$WORK/producer.log" 2>/dev/null
    stop_producer
}

s_simple() { start_producer && consumer 7001 "$1"; }

s_concurrent() {
    start_producer || return 1
    local pids=() ok=0
    for i in 1 2 3 4 5 6 7 8; do consumer $((7100+i)) seq > "$WORK/c$i.log" 2>&1 & pids+=($!); done
    for i in "${!pids[@]}"; do wait "${pids[$i]}" && ok=$((ok+1)); done
    grep -h "FAIL\|HANG" "$WORK"/c*.log | head -5
    echo "concurrent consumers passed: $ok/8"
    [ "$ok" -eq 8 ]
}

s_overflow() {
    start_producer || return 1
    local pids=() ok=0
    for i in $(seq 1 8); do consumer $((7200+i)) slow1 > "$WORK/o$i.log" 2>&1 & pids+=($!); done
    for i in "${!pids[@]}"; do wait "${pids[$i]}" && ok=$((ok+1)); done
    local served refused
    served=$(grep -l "OK slow call" "$WORK"/o*.log | wc -l)
    refused=$(grep -l "DEFINED-ERROR" "$WORK"/o*.log | wc -l)
    grep -h "FAIL\|HANG\|DEFINED-ERROR" "$WORK"/o*.log | head -8
    echo "slow calls: $served served, $refused refused with an error, $ok/8 processes ended cleanly"
    [ "$ok" -eq 8 ] && [ "$served" -ge 1 ]
}

s_queue() { start_producer && consumer 7701 queue; }

s_handler_panic() {
    start_producer || return 1
    consumer 7801 handler_panic; local r=$?
    if kill -0 "$PRODUCER" 2>/dev/null; then echo "INFO producer still running after the handler panic"
    else echo "INFO producer ended after the handler panic"; PRODUCER=""; fi
    return $r
}

# Misconfiguration: two consumer processes with the same applicationID. Calls may fail, but a
# wrong answer is a failure.
s_dup_appid() {
    start_producer || return 1
    consumer 7901 integrity > "$WORK/d1.log" 2>&1 & local a=$!
    consumer 7901 integrity > "$WORK/d2.log" 2>&1 & local b=$!
    local r=0; wait $a || r=1; wait $b || r=1
    grep -h "INFO integrity\|FAIL\|HANG" "$WORK/d1.log" "$WORK/d2.log"; return $r
}

# Misconfiguration: a consumer with the producer's applicationID (the first run's wrong value
# happened when every process shared one). Calls may fail, a wrong answer is a failure.
s_dup_appid_producer() {
    start_producer || return 1
    consumer $PRODUCER_APP integrity > "$WORK/p1.log" 2>&1 & local a=$!
    consumer $PRODUCER_APP integrity > "$WORK/p2.log" 2>&1 & local b=$!
    local r=0; wait $a || r=1; wait $b || r=1
    grep -h "INFO integrity\|FAIL\|HANG" "$WORK/p1.log" "$WORK/p2.log"; return $r
}

s_two_consumers() {
    start_producer || return 1
    consumer 7301 seq > "$WORK/t1.log" 2>&1 & local a=$!
    consumer 7302 seq > "$WORK/t2.log" 2>&1 & local b=$!
    local r=0; wait $a || r=1; wait $b || r=1
    tail -n 3 "$WORK/t1.log" "$WORK/t2.log"; return $r
}

s_early() {
    consumer 7401 early > "$WORK/early.log" 2>&1 & local c=$!
    sleep 3
    start_producer || return 1
    local r=0; wait $c || r=1; cat "$WORK/early.log"; return $r
}

s_crash() {
    mkdir -p "$WORK/sync"; rm -f "$WORK/sync"/*
    start_producer || return 1
    E2E_SYNC_DIR="$WORK/sync" consumer 7501 crash > "$WORK/crash.log" 2>&1 & local c=$!
    for _ in $(seq 1 200); do [ -e "$WORK/sync/ready" ] && break; sleep 0.1; done
    kill -9 "$PRODUCER"; wait "$PRODUCER" 2>/dev/null; PRODUCER=""
    touch "$WORK/sync/killed"
    sleep 2
    start_producer || return 1
    touch "$WORK/sync/restarted"
    local r=0; wait $c || r=1; cat "$WORK/crash.log"; return $r
}

for s in "${SCENARIOS[@]}"; do
    case "$s" in
        basic|seq) run "$s" s_simple "$s" ;;
        concurrent) run concurrent s_concurrent ;;
        overflow) run overflow s_overflow ;;
        two_consumers) run two_consumers s_two_consumers ;;
        queue) run queue s_queue ;;
        handler_panic) run handler_panic s_handler_panic ;;
        dup_appid) run dup_appid s_dup_appid ;;
        dup_appid_producer) run dup_appid_producer s_dup_appid_producer ;;
        early) run early s_early ;;
        crash) run crash s_crash ;;
        *) echo "unknown scenario $s"; RESULT[$s]=FAIL ;;
    esac
done

echo; echo "=================== SUMMARY"
rc=0
for s in "${SCENARIOS[@]}"; do echo "$s: ${RESULT[$s]:-?}"; [ "${RESULT[$s]:-FAIL}" = FAIL ] && rc=1; done
exit $rc
