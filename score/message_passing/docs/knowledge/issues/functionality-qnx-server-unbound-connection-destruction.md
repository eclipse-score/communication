<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Review Destruction Before QNX Server Connection Binding

Status: needs-review
Area: functionality
Scope: pending ServerConnection destruction after admission or attachment failure
Owner: unassigned
Evidence checked: 2026-11-23; source and existing-test inspection, not a new runtime reproduction

## Finding

`ProcessConnect` constructs a locally owned connection before invoking the
application's connect callback. Native OCB attachment happens only after that
callback succeeds. Application rejection or attachment failure returns through
destruction of the locally owned pending object, with `user_data_` still empty.

The connection destructor calls `GetQnxDispatchServer` before checking whether
`user_data_` is engaged. That lookup follows `GetServer` and `OcbToServer`,
which dereferences the OCB's attribute pointer as a bound server.
The native base was zero-initialized, and application rejection occurs before
any attachment can supply that binding.

The UserData guard therefore does not guard this earlier binding-dependent
lookup. This is a source-level cleanup defect to investigate, not a newly
reproduced crash or permission to expose every connection API during admission.
Whether attachment failure leaves any partial binding must be checked
separately from rejection before attachment.

## Evidence

- [Server implementation](../../../qnx_dispatch/qnx_dispatch_server.cpp):
  `ProcessConnect`, `AcceptConnection` and the connection destructor.
- [Server declarations](../../../qnx_dispatch/qnx_dispatch_server.h):
  `GetQnxDispatchServer` delegates to the native base lookup.
- [Engine declarations](../../../qnx_dispatch/qnx_dispatch_engine.h):
  `ResourceManagerConnection` zero-initialization, `GetServer` and `OcbToServer`.
- [Mocked server tests](../../../qnx_dispatch_mocked_server_test.cpp):
  `ServerOpenConnectOcbAttachFailure` exercises an attachment error.
  [Native integration tests](../../../qnx_dispatch_server_to_client_test.cpp)
  include application rejection. Passing these ordinary cases does not prove
  that an unbound server-reference expression is valid.

## Next Check

Use an instrumented binding invariant or a suitable sanitizer/checker to detect
server lookup before attachment. Cover application rejection and native
attachment failure independently, including pending objects with queued early
notifications. Establish which destruction work actually needs a bound server
before changing the lookup order or ownership model.

Keep the runtime correction separate from issue recording and retain successful
admission/disconnect behavior when adding a focused regression.
