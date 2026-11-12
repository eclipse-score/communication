<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Recover Failure-Containment Intent Without Inventing Guarantees

Status: proposed; recovered intent requiring safety-review confirmation
Date: 2026-11-04
Scope: QNX runtime failures, containment, and integrator obligations

## Context

Earlier safety discussions rejected a uniform "safe-silent" placeholder and
asked for plain-language, implementation-grounded failure handling. The
following preserves that intent for review, not a claim that every path already
satisfies it or that a formal safe state has been agreed.

## Intent to Confirm

1. Report failure when the API permits it; distinguish local rejection from a
   later asynchronous error or connection-state change.
2. Where no error result exists, describe the actual silence/containment rather
   than inventing a reporting channel.
3. Preserve the ordering promised by the chosen configuration.
4. On send failure, clarify the reported-error versus channel-silence/reconnect
   behavior for the actual path; do not infer delivery from local acceptance.
5. Isolate a failing channel/activity when that is feasible. A new connection's
   resource failure should not gratuitously invalidate existing connections.
6. Termination may be necessary when containment cannot be guaranteed, including
   exhaustion of the element's own resources. Startup and runtime failure points
   need separate analysis; this is not a promise of universal runtime recovery.
7. Keep AoUs for same-process contract misuse proportionate. Do not invent an
   obligation for the integrator to terminate on the library's behalf.
8. Peer misbehavior should not crash the process; confirm and document whether
   a path ignores one message, drops a connection, or takes another action.
   Earlier review found inconsistent handling, not one uniform existing policy.
9. An AoU and a partial internal mitigation can coexist. Internal buffering does
   not by itself establish an end-to-end timing guarantee.

## Consequences and Evidence Boundary

Read the relevant QNX implementation before making a formal measure or AoU.
[ClientConnection](../../../client_connection.cpp),
[QNX dispatch server](../../../qnx_dispatch/qnx_dispatch_server.cpp), and
[API contracts](../../../i_client_connection.h) are evidence starting points,
not proof that all the proposed intent holds. Empty required-callback behavior
and exact per-failure containment boundaries still need explicit review.

The original discussion is recovered context without a current approval PR or
complete per-path analysis. Confirm it in the scheduled safety work, alongside
[the completeness findings](../issues/safety-analysis-completeness.md), rather
than silently turning it into normative TRLC or a blanket fail-stop claim.