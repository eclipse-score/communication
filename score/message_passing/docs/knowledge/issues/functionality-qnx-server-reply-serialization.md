<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Establish QNX Server Reply Serialization and Record Reuse

Status: needs-review
Area: functionality
Scope: Reply payload access, native read retirement and single-record membership
Owner: unassigned
Evidence checked: 2026-11-23; source/contracts inspection, not a reproduced race

## Finding

Each connection has one embedded `reply_message_`. `Reply` copies the payload
into that record before acquiring `send_mutex_`, then appends the same record
to the outgoing intrusive FIFO without checking whether it is already linked.

`ProcessReadRequest` accesses the queued record and its payload under the mutex
through the native `MsgReplyv` attempt, then unlinks it. That lock does not
protect the preceding payload copy in another `Reply` invocation. A repeated
reply before retirement can also reuse the same storage and attempt duplicate
intrusive membership.

The supported reply discipline needs an explicit argument: which cross-thread,
overlapping or repeated calls are permitted, and how record ownership is held
until native consumption finishes. Per-session serialization of server
callbacks alone does not establish serialization of arbitrary external
connection-method callers.

These are source-backed access and membership concerns, not a reproduced
supported-call data race, list failure or an accepted new API precondition.

## Evidence

- [Server implementation](../../../qnx_dispatch/qnx_dispatch_server.cpp):
  `Reply` and `ProcessReadRequest`.
- [Server declarations](../../../qnx_dispatch/qnx_dispatch_server.h):
  one reply record, the queue mutex and its cross-thread access comment.
- [Server contract](../../../i_server.h): per-session callback serialization.
  [Connection interface](../../../i_server_connection.h) does not state a
  detailed repeated/concurrent reply discipline.
- [Integration tests](../../../qnx_dispatch_server_to_client_test.cpp) exercise
  ordinary request/reply paths, not explicitly held native reads overlapping
  another reply or repeated queuing of the same record.

## Next Check

First agree the permitted public call sequences. Use explicit synchronization
to hold a native read/reply attempt while another permitted caller reaches
reply admission, and check storage access and retirement ordering. Separately
exercise a second reply before the first record is consumed, with a bounded
diagnostic for intrusive-membership rejection.

Do not treat moving one lock as sufficient without checking payload lifetime,
pending-record ownership and unchanged request/reply semantics.
