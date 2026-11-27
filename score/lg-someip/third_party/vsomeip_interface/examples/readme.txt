# Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

To use the example applications, configure the unicast address and service
addresses for the host or hosts running the applications. The checked-in
configuration is available at `../../../../examples/config/lgsomeip_config.json`
when these examples are run from the generated build directory.

To start the request/response-example from the build-directory do:

HOST1: env VSOMEIP_CONFIGURATION=../../../../examples/config/lgsomeip_config.json VSOMEIP_APPLICATION_NAME=request-1 ./request-sample
HOST1: env VSOMEIP_CONFIGURATION=../../../../examples/config/lgsomeip_config.json VSOMEIP_APPLICATION_NAME=response ./response-sample

To start the subscribe/notify-example from the build-directory do:

HOST1: env VSOMEIP_CONFIGURATION=../../../../examples/config/lgsomeip_config.json VSOMEIP_APPLICATION_NAME=subscribe-1 ./subscribe-sample
HOST1: env VSOMEIP_CONFIGURATION=../../../../examples/config/lgsomeip_config.json VSOMEIP_APPLICATION_NAME=notify ./notify-sample
