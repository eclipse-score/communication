#!/bin/bash
# *******************************************************************************
# Copyright (C) 2017-2026 LG Electronics Inc.
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

killall -9 someip-daemon response-app
rm -rf /tmp/someip/

sleep 1

./someip-daemon &

sleep 1

for i in $(seq $1)
do
    ./response-app $i &
    sleep 1
done
