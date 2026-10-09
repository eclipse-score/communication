<!--
Copyright (c) 2026 Contributors to the Eclipse Foundation

See the NOTICE file(s) distributed with this work for additional
information regarding copyright ownership.

This program and the accompanying materials are made available under the
terms of the Apache License Version 2.0 which is available at
https://www.apache.org/licenses/LICENSE-2.0

SPDX-License-Identifier: Apache-2.0
-->

# COM API — `futures` crate usage review

Detailed-design record for issue #1263. It covers the `futures` crate as used by the Rust COM API.

Items marked **Open** require input or verification that is not available from this repository.

## Contents

- [Pinned version and features](#pinned-version-and-features)
- [Direct call sites](#direct-call-sites)
- [Crate review](#crate-review)
- [Reduction analysis](#reduction-analysis)
- [Decision](#decision)
- [Qualification artifacts](#qualification-artifacts)

## Pinned version and features

| Item | Value | Source |
|------|-------|--------|
| Version | `0.3.34` | `MODULE.bazel.lock` (`crate_index__futures-0.3.34`) |
| Archive | `https://static.crates.io/crates/futures/0.3.34/download` | `MODULE.bazel.lock` |
| SHA-256 | `9a31d2a3fbaaeb2af2368bbdd904aa8e812d3c04a1ee10d3171f52d556e5d0a3` | `MODULE.bazel.lock` |
| Enabled features | `alloc`, `async-await`, `default`, `executor`, `futures-executor`, `std` | `MODULE.bazel.lock` (`rust_library` `futures`) |
| Direct sub-crates | `futures-channel`, `futures-core`, `futures-executor`, `futures-io`, `futures-sink`, `futures-task`, `futures-util` | `MODULE.bazel.lock` |
| Resolved through | `bazel_dep(name = "score_crates", version = "0.0.11", repo_name = "score_communication_crate_index")` | `MODULE.bazel` |

The version and feature set come from the external `score_crates` module. Changing them is done there, not in this repository.

Targets that declare `@score_communication_crate_index//:futures`:

| Target                                                       | Line       |
| ------------------------------------------------------------ | ---------- |
| `score/mw/com/impl/rust/com-api/com-api-runtime-lola/BUILD`  | 32         |
| `score/mw/com/impl/rust/com-api/com-api-runtime-mock/BUILD`  | 22         |
| `score/mw/com/rust/score_com_concept/BUILD`                  | 30         |
| `score/mw/com/example/com-api-example/BUILD`                 | 26, 48, 71 |
| `score/mw/com/test/basic_rust_api/consumer_async_apis/BUILD` | 39         |

## Direct call sites

### Production runtime and public API

| File | Lines | Items used |
|------|-------|------------|
| `score/mw/com/rust/score_com_concept/concept.rs` | 58 | `futures::stream::Stream` (public `Subscription::to_stream()` return type) |
| `score/mw/com/impl/rust/com-api/com-api-runtime-lola/consumer.rs` | 40–41, 958, 1031 | `stream::Stream`, `task::{AtomicWaker, Context, Poll}` (`AtomicWaker` wakes pending receives and discovery futures) |

No `executor` item is used in production code.

### Mock and test-only

| File                                                                   | Lines      | Items used                                                         |
| ---------------------------------------------------------------------- | ---------- | ------------------------------------------------------------------ |
| `score/mw/com/impl/rust/com-api/com-api-runtime-mock/runtime.rs`       | 34, 538    | `stream::{self, Stream}`, `executor::block_on`                     |
| `score/mw/com/example/com-api-example/main.rs`                         | 110–151    | `executor::block_on`                                               |
| `score/mw/com/example/com-api-example/src/consumer.rs`                 | 19–20      | `channel::oneshot`, `FutureExt`, `StreamExt`                       |
| `score/mw/com/example/com-api-example/tests_using_tokio_runtime.rs`    | 41         | `stream::StreamExt`                                                |
| `score/mw/com/test/basic_rust_api/consumer_async_apis/consumer_app.rs` | 35–36, 187 | `channel::oneshot`, `FutureExt`, `StreamExt`, `executor::block_on` |

### Documentation only

`score/mw/com/rust/doc/user_facing_api_examples.md` (lines 342–343, 403) and `score/mw/com/example/com-api-example/USAGE.md` (line 47).

The current repository evidence does not show the COM API imposing a specific executor on application users. Executor usage identified in this repository is limited to the mock, examples, and tests.

## Crate review

| Topic | Status |
|-------|--------|
| Provenance | crates.io archive pinned by SHA-256 (see above). |
| License | Declared in the crate metadata. **Open:** confirm against the pinned archive and record the text. |
| Maintenance | **Open:** record upstream release cadence and maintainer status at review time. |
| Safety relevance | `Stream` and `AtomicWaker` are on the production receive path (`consumer.rs`). **Open:** record the safety analysis for these two items, and whether any `unsafe` inside them is in scope. |

## Reduction Analysis

Potential areas for reducing the `futures` dependency usage were reviewed:

| Usage site | Feasibility | Impact | Recommendation |
| ---------- | ----------- | ------ | -------------- |
| **`Stream` trait (public API)** | Cannot remove without an API change | `Subscription::to_stream()` exposes `futures::Stream` | **RETAIN** |
| **`AtomicWaker` (production)**       | Dependency reduction may be possible   | Required by the LoLa runtime for waking pending receives and discovery futures | **EVALUATE smaller sub-crate** |
| **`Context` / `Poll` (production)**  | Potentially reducible                  | These polling types can potentially be obtained directly from `std::task`      | **VERIFY before changing**     |
| **Example `block_on`**               |  Possible                               | Used only by examples                                                          | **OPTIONAL REFACTORING**       |
| **Mock/test `block_on` and helpers** |  Possible                               | Used by mock/tests/examples                                                    | **EVALUATE SEPARATELY**        |

### Reduction conclusion

The `futures` dependency cannot be removed completely from the current public API without changing `Subscription::to_stream()` and its `futures::Stream` return type.

However, the **production dependency footprint may be reducible** by depending directly on the required `futures-*` sub-crates instead of the full `futures` crate.

This requires:

- verification of the required APIs, particularly `AtomicWaker`;
- verification of whether `Context` and `Poll` can be imported directly from `std::task` without the `futures` dependency;
- a corresponding change to the external `score_crates` configuration; and
- validation that the resulting dependency set builds and passes the relevant tests.

This reduction should be treated as a separate optimization from the current decision to retain the `futures` API used by the COM API.

## Decision

**Proposed (needs maintainer approval): retain `futures`.**

- `Subscription::to_stream()` exposes `futures::Stream`, so removing or replacing the crate would require a public API change.
- Production code uses `Stream`, `AtomicWaker`, `Context`, and `Poll`. The `executor` functionality is not used by the production runtime; the identified executor usage is limited to the mock, examples, and tests.
- A dependency-size reduction can be evaluated separately by depending on the required smaller `futures-*` sub-crates in production while retaining the full crate where required by mock/test/example code. This requires a change in `score_crates` and verification of which sub-crate provides each required API, including `AtomicWaker`.
- Replacing `futures` entirely is not recommended unless the public stream API and/or runtime implementation is intentionally redesigned.

## Qualification artifacts

Record for the retained crate:

1. The pinned archive hash and enabled feature list above.
2. The license text from the pinned archive.
3. The maintenance review from the crate review table.
4. The safety analysis for `Stream` and `AtomicWaker` use in [consumer.rs](../../impl/rust/com-api/com-api-runtime-lola/consumer.rs).
5. The final decision and its approver.
