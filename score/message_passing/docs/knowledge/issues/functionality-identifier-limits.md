<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Establish Identifier Limits and Validation Behavior

Status: open
Area: functionality
Scope: service identifiers in QNX dispatch and Unix Domain
Owner: functionality and requirements reviewers; assignment pending
Evidence checked: 2026-11-04; source inspection, not new runtime tests

## Finding

[ServiceProtocolConfig](../../../service_protocol_config.h) does not document
a portable identifier length limit. [QnxResourcePath](../../../qnx_dispatch/qnx_resource_path.h)
has a 256-byte maximum and a nonempty/bounded-input precondition enforced in
[its constructor](../../../qnx_dispatch/qnx_resource_path.cpp).
[Existing tests](../../../qnx_resource_path_test.cpp) cover QNX path boundaries.

[UnixDomainSocketAddress](../../../unix_domain/unix_domain_socket_address.h)
copies at most `sizeof(sun_path) - 2` identifier bytes for an abstract address,
normally 106 on Linux, without reporting truncation. Identifiers differing only
after that prefix can map to the same address. This is not the QNX path behavior;
do not infer a shared limit or validation policy.

Earlier small-string-optimization suggestions were allocator/library-specific
advice, not an agreed identifier contract.

## Next Check

Agree backend-specific and portable identifier contracts with the functionality
and requirements owners. Add discriminating tests for agreed boundary and
truncation behavior before claiming conformance.