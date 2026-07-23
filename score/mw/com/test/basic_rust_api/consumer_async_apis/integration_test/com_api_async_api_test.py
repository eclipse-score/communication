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

from contextlib import contextmanager


def producer(target, num_cycles, **kwargs):
    args = ["-n", str(num_cycles)]
    return target.wrap_exec("bin/bigdata-producer", args, cwd="/opt/bigdata-com-api-async", **kwargs)


def consumer_async_with_cancellation(target, num_cycles, **kwargs):
    """Consumer using cancellable_receive API with timeout - exits after num_cycles"""
    args = ["-n", str(num_cycles), "-m", "with-cancellation"]
    return target.wrap_exec(
        "bin/bigdata-consumer-async", args, cwd="/opt/bigdata-com-api-async", wait_on_exit=True, **kwargs
    )


def consumer_async_without_cancellation(target, num_cycles, **kwargs):
    """Consumer using async receive API - exits after num_cycles"""
    args = ["-n", str(num_cycles), "-m", "without-cancellation"]
    return target.wrap_exec(
        "bin/bigdata-consumer-async", args, cwd="/opt/bigdata-com-api-async", wait_on_exit=True, **kwargs
    )


def consumer_async_stream(target, num_cycles, **kwargs):
    """Consumer using async stream API - exits after num_cycles"""
    args = ["-n", str(num_cycles), "-m", "stream"]
    return target.wrap_exec(
        "bin/bigdata-consumer-async", args, cwd="/opt/bigdata-com-api-async", wait_on_exit=True, **kwargs
    )


def service_discovery_daemon(target, **kwargs):
    del kwargs

    @contextmanager
    def _service_discovery_daemon():
        daemon_process = target.execute_async(
            "bin/service_discovery_daemon_app",
            args=[],
            cwd="/opt/ServiceDiscoveryDaemonApp",
        )
        yield daemon_process

    return _service_discovery_daemon()


def test_bigdata_async_with_cancellation(target):
    """Test async receive with cancellable_receive API.
    
    Producer sends 40 samples, consumer receives 25 using cancellable_receive
    with 50ms timeout (using futures::channel::oneshot) and exits cleanly after reaching num_cycles.
    Producer sends sample with 100ms interval, so consumer will timeout intermittently while waiting for samples, demonstrating the cancellation behavior.
    """
    with service_discovery_daemon(target), producer(target, num_cycles=40), consumer_async_with_cancellation(
        target, num_cycles=25, wait_timeout=120
    ):
        pass


def test_bigdata_async_without_cancellation(target):
    """Test async receive without cancellation API.
    
    Producer sends 40 samples, consumer receives 25 using standard async
    receive and exits cleanly after reaching num_cycles.
    """
    with service_discovery_daemon(target), producer(target, num_cycles=40), consumer_async_without_cancellation(
        target, num_cycles=25, wait_timeout=120
    ):
        pass


def test_bigdata_async_stream(target):
    """Test async receive using stream API.
    
    Producer sends 40 samples, consumer receives 25 using async stream
    and exits cleanly after reaching num_cycles.
    """
    with service_discovery_daemon(target):
        with producer(target, num_cycles=40, wait_on_exit=True, wait_timeout=120):
            with consumer_async_stream(target, num_cycles=25, wait_timeout=120):
                pass
