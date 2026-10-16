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


def api_idempotency_app(target, **kwargs):
    args = ["--service_instance_manifest", "./etc/mw_com_config.json"]
    return target.wrap_exec(
        "bin/main_api_idempotency",
        args,
        cwd="/opt/ApiIdempotencyApp",
        wait_on_exit=True,
        **kwargs,
    )


def test_api_idempotency(target):
    """Integration test for idempotent public API calls.

    The test covers OfferService, StopOfferService, StartFindService, Subscribe
    and Unsubscribe. It verifies duplicate offers keep the service discoverable,
    repeated stops remove discovery, repeated StartFindService registrations
    discover the service and are cleaned up, duplicate Subscribe with the same
    sample limit preserves subscription and sample reception, and repeated
    Unsubscribe leaves the event unsubscribed so re-subscription still works.
    """
    with api_idempotency_app(target) as application:
        pass
    assert application.ret_code == 0, (
        f"API idempotency application must complete successfully; exit code: {application.ret_code}"
    )
