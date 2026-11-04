# Establish Allocation-Containment Evidence

Status: open
Area: functionality
Scope: library allocations in common client, QNX dispatch, and Unix Domain
Owner: functionality and requirements reviewers; assignment pending
Evidence checked: 2026-11-04; source inspection, not new runtime tests

## Finding

Each [Unix Domain engine](../../../unix_domain/unix_domain_engine.cpp) and
[QNX engine](../../../qnx_dispatch/qnx_dispatch_engine.cpp) has a grow-only
`posix_receive_buffer_` shared across registered POSIX endpoints. QNX reserves
an additional protocol-code byte. This observation does not describe every
native-QNX receive allocation or kernel resource.

Inspected [client fixtures](../../../client_connection_test.cpp) and
[resource-manager fixtures](../../../resource_manager_fixture_base.h) use the
process default memory resource. Those fixtures alone cannot distinguish correct
injection from fallback to that same resource. No containment proof was located
in the inspected component tests/helpers; repository-wide absence is not claimed.

## Next Check

Consider an injected counting resource and a failing default resource, with
suitable isolation/restoration; distinguish library allocations from OS
resources. This is a test proposal, not an existing containment proof.