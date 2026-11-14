# Bundled Third-Party Sources

This directory contains source trees derived from external open-source projects.
Each component retains its original copyright notices and license text.

## vsomeip Compatibility Layer

`vsomeip_interface/` contains the bundled vsomeip-compatible interface, adapter
implementation, and examples. Its source files retain their original BMW AG
MPL-2.0 notices. The supported application API is exposed through
`include/vsomeip` and the `//score/lg-someip:vsomeip-if` Bazel target.

## E2E Protection

`e2e_protection/` contains the bundled E2E protection implementation and its
profiles. Its source files retain their original BMW AG MPL-2.0 notices. The
E2E headers are internal implementation details and are not exposed as public
headers.

See [THIRD_PARTY_LICENSES.md](../THIRD_PARTY_LICENSES.md) for the complete
license and attribution details.
