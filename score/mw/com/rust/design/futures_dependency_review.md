<!--
Copyright (c) 2026 Contributors to the Eclipse Foundation

See the NOTICE file(s) distributed with this work for additional
information regarding copyright ownership.

This program and the accompanying materials are made available under the
terms of the Apache License Version 2.0 which is available at
https://www.apache.org/licenses/LICENSE-2.0

SPDX-License-Identifier: Apache-2.0
-->
# COM API — `futures` crate usage review and component classification

- Component: `futures` crate, classified as a software component in the context of S-CORE
- Repository: https://github.com/rust-lang/futures-rs
- Documentation: https://docs.rs/futures/0.3.34/futures/
- Crate version: 0.3.34 (pinned in `MODULE.bazel.lock`)
- Additional documentation considered: https://crates.io/crates/futures

Sections 1-10 assess the COM API integration. Section 11 is the crate-level classification and is kept separate.


Detailed-design record for issue #1263 It reviews the `futures` crate as used by the Rust COM API and assesses how well the current setup supports safety-related artifacts and future qualification.

All statements were checked against this repository. Items marked **Open** need input or evidence that is not available from the repository (upstream data, stakeholder decisions, CI run results).

## Contents

1. [Pinned Version and Features](#1-pinned-version-and-features)
2. [Direct Call Sites](#2-direct-call-sites)
3. [Crate Review](#3-crate-review)
4. [Reduction Analysis](#4-reduction-analysis)
5. [Decision](#5-decision)
6. [Safety Artifact Assessment](#6-safety-artifact-assessment)
7. [Qualification Feasibility Assessment](#7-qualification-feasibility-assessment)
8. [Recommendation](#8-recommendation)
9. [Next Steps](#9-next-steps)
10. [Acceptance Criteria Mapping and Review Record](#10-acceptance-criteria-mapping-and-review-record)
11. [Crate-Level Classification](#11-crate-level-classification)

## 1. Pinned Version and Features

| Item | Value | Source |
|------|-------|--------|
| Version | `0.3.34` | `MODULE.bazel.lock` (`crate_index__futures-0.3.34`) |
| Archive | `https://static.crates.io/crates/futures/0.3.34/download` | `MODULE.bazel.lock` |
| SHA-256 | `9a31d2a3fbaaeb2af2368bbdd904aa8e812d3c04a1ee10d3171f52d556e5d0a3` | `MODULE.bazel.lock` |
| Enabled features | `alloc`, `async-await`, `default`, `executor`, `futures-executor`, `std` | `MODULE.bazel.lock` (`rust_library` target for `futures`) |
| Direct sub-crates | `futures-channel`, `futures-core`, `futures-executor`, `futures-io`, `futures-sink`, `futures-task`, `futures-util` | `MODULE.bazel.lock` |
| Resolved through | `bazel_dep(name = "score_crates", version = "0.0.13", repo_name = "score_communication_crate_index")` | `MODULE.bazel` |

The version and feature set come from the external `score_crates` module. Changing them is done there, not in this repository.

Targets that declare `@score_communication_crate_index//:futures`:

| Target | Line |
|--------|------|
| `score/mw/com/impl/rust/com-api/com-api-runtime-lola/BUILD` | 32 |
| `score/mw/com/impl/rust/com-api/com-api-runtime-mock/BUILD` | 22 |
| `score/mw/com/rust/score_com_concept/BUILD` | 30 |
| `score/mw/com/example/com-api-example/BUILD` | 26, 48, 71 |
| `score/mw/com/test/basic_rust_api/consumer_async_apis/BUILD` | 39 |

## 2. Direct Call Sites

### Production runtime and public API

| File | Lines | Items used |
|------|-------|------------|
| `score/mw/com/rust/score_com_concept/concept.rs` | 57, 931 | `futures::stream::Stream` — return type of `Subscription::to_stream()` |
| `score/mw/com/impl/rust/com-api/com-api-runtime-lola/consumer.rs` | 40–41 | imports `stream::Stream`, `task::{AtomicWaker, Context, Poll}` |
| same | 487, 699 | `Arc<AtomicWaker>` in the subscriber and `ReceiveFuture` |
| same | 676 | `to_stream()` implementation (`SampleStream: Stream`) |
| same | 925, 993 | `AtomicWaker::new()` / field in the service-discovery future |

`AtomicWaker` usage pattern in the production code:

Verified by reading `consumer.rs` (source inspection only; no test or run evidence):

- `ReceiveFuture::poll` (line 713) polls the cancellation future first and may return `Ready` without registering (lines 721–727). Otherwise it calls `register(ctx.waker())` (line 730) before `try_receive_samples`.
- `SampleStream::poll_next` (line 794) returns a buffered sample early without registering (lines 795–797). Otherwise it calls `register(cx.waker())` (line 803) before `try_receive_samples`.
- `ServiceDiscoveryFuture::poll` (line 1014) calls `register(ctx.waker())` (line 1016) before locking `discovery_state` and checking the handles.
- The discovery callback (a Rust closure, lines 938–943, invoked by the C++ side) stores the handles inside `if let Ok(mut state) = state_ref.lock()` and then calls `wake()` (line 942) after that block. If the lock is poisoned, the handles are not stored but `wake()` is still called.
- The subscriber receive callback (line 517) only calls `wake()`.

**Open:** the memory-ordering and synchronization guarantees of `AtomicWaker` across threads are not analysed here. The upstream rustdoc of `AtomicWaker` (`futures-core` 0.3.34, `atomic_waker.rs`) states that consumers should call `register` before checking the result and producers `wake` after producing it, that `wake` before `register` is a no-op, and that `register` acquires memory released by earlier `wake` calls. This matches the order in the COM code above, but the documentation is a claim, not a verification; the "no missed wake-up" property is not tested or analysed.

No `futures::executor` usage was found in production code. The COM API does not impose an executor on applications.

### Mock, examples, tests

| File | Lines | Items used |
|------|-------|------------|
| `score/mw/com/impl/rust/com-api/com-api-runtime-mock/runtime.rs` | 34, 338, 529 | `stream::{self, Stream}`, `stream::empty()`, `executor::block_on` (in a unit test) |
| `score/mw/com/example/com-api-example/main.rs` | 106–143 | `executor::block_on` |
| `score/mw/com/example/com-api-example/src/consumer.rs` | 15–16 | `channel::oneshot`, `FutureExt`, `StreamExt` |
| `score/mw/com/example/com-api-example/tests_using_tokio_runtime.rs` | 41 | `stream::StreamExt` (tokio runtime) |
| `score/mw/com/test/basic_rust_api/consumer_async_apis/consumer_app.rs` | 31–32, 182 | `channel::oneshot`, `FutureExt`, `StreamExt`, `executor::block_on` |

### Documentation references

- `score/mw/com/rust/doc/user_facing_api_examples.md` (lines 342–343, 403)
- `score/mw/com/example/com-api-example/USAGE.md` (line 47)
- [The Rust Programming Language — Streams](https://doc.rust-lang.org/book/ch17-04-streams.html) — background on stream-based asynchronous programming in Rust.

## 3. Crate Review

| Topic | Current assessment | Status |
|-------|--------------------|--------|
| Provenance | Pinned by version and SHA-256 in `MODULE.bazel.lock`. Fetched from `static.crates.io`. The SHA-256 of the downloaded archives of `futures`, the seven `futures-*` crates listed in 11.4, `futures-macro`, `memchr`, `slab` and `pin-project-lite` was found in `MODULE.bazel.lock`. | Partly verified (archive hash equals lock entry; this does not verify against upstream source); **Open:** comparison with upstream tag `0.3.34`, and verification of `proc-macro2`, `quote`, `syn`, `unicode-ident` |
| Licensing | `license = "MIT OR Apache-2.0"` in the `Cargo.toml` of the pinned `futures-core` and `futures-util` archives; the `futures-core` archive ships `LICENSE-APACHE` and `LICENSE-MIT`. The `Cargo.toml` license field and license files were also seen in the downloaded archives of the other `futures-*` crates, `futures-macro` (all `MIT OR Apache-2.0`, with `LICENSE-APACHE` and `LICENSE-MIT`), `memchr` (`Unlicense OR MIT`, `LICENSE-MIT`, `UNLICENSE`), `pin-project-lite` (`Apache-2.0 OR MIT`, both files) and `slab` (`MIT`, `LICENSE`). License texts are not committed to this repository, and `proc-macro2`, `quote`, `syn`, `unicode-ident` were not checked. | Declared licenses and license files seen in archives; **Open:** check the four remaining closure crates, collect license texts, and confirm the license review approach with the Safety team |
| Maintenance | Upstream is `rust-lang/futures-rs`. crates.io and GitHub list `0.3.34` as the latest release (published 2026-08-11). The RustSec advisory-db contains `RUSTSEC-2020-0059` to `-0062` for `futures` crates; for `futures-util` two were read (patched in `>= 0.3.7` and `>= 0.3.2`, both older than `0.3.34`); the others and the per-crate listings were not fully checked (unauthenticated GitHub API, possible rate limits). Repository activity and maintainers were not reviewed. | **Open:** complete dated activity/advisory review |
| Safety relevance | `Stream`, `AtomicWaker`, `Context`, `Poll` are on the production receive and discovery paths. | **Open:** focused analysis (see section 6) |
| API stability | `futures::Stream` is part of the public API; upgrades must stay source-compatible. | Document compatibility rule |
| Build reproducibility | Built through Bazel/`rules_rust` with the Ferrocene toolchain. | **Open:** record an offline/clean-room rebuild |

### Provenance
The lock file gives version and archive hash. It does not cover publisher identity or review history of the crate. Effort to record and verify: **Low**.

### Licensing
License file extraction from the pinned archive is straightforward. Effort: **Low**.

### Maintenance
Needs a dated snapshot: latest release, release history, repository activity, maintainers, RustSec advisories. Popularity is not evidence of maintenance guarantees. Effort: **Low–Medium** (one-off), **Medium** if made recurring.

### Safety relevance
Areas to analyse:

1. **`AtomicWaker`:** registration/wake concurrency, lifecycle, memory-ordering contract, and use of `unsafe` inside the crate.
2. **`Stream`:** termination, pending behaviour, error items, cancellation, and the exclusive-access guard in the stream.
3. **`Context` / `Poll`:** these are re-exports of `core::task` types, so they add no independent behaviour.
4. **COM API own `unsafe`:** the `mem::transmute` of the discovery callback and `stop_find_service` in `Drop` (`consumer.rs`) are COM-owned and outside the `futures` crate, but belong in the same analysis.
5. **Panics in poll paths:** several `expect(...)` calls exist (for example the discovery `Mutex` lock and the `scratch` container). Their reachability should be assessed.

Missed wake-ups, stalled operations or concurrent access are failure modes to evaluate, not demonstrated defects.

## 4. Reduction Analysis

| Usage site | Feasibility | Impact | Recommendation |
|------------|-------------|--------|----------------|
| `Stream` trait (public API) | Cannot be removed without an API change | `Subscription::to_stream()` exposes `futures::Stream` | **Retain** |
| `AtomicWaker` (production) | Possible via a smaller sub-crate | Needed for wake-up of pending receives and discovery | **Evaluate sub-crate** |
| `Context` / `Poll` (production) | Directly replaceable | Available in `std::task` | **Switch to `std::task`** after build check |
| Example `block_on` | Replaceable | Example code only | Optional |
| Mock and test helpers | Replaceable | Mock, test and example code only | Evaluate separately |

### Reduction conclusion

The public `Stream` return type keeps a `futures` ecosystem dependency. `Stream` is defined in `futures-core` (`src/stream.rs`). `AtomicWaker` is defined in `futures-core` under `task::__internal` (a `#[doc(hidden)]` module); its public path is `futures_util::task::AtomicWaker`, and `futures::task` is `futures_util::task` (`futures-0.3.34/src/lib.rs`, line 131). A narrower production dependency therefore means either `futures-util` (about 16k lines) or the hidden `__internal` path of `futures-core`, which carries no stability promise. `Context` and `Poll` are `core::task` re-exports, so `std::task` provides the same types. The full `futures` crate (with `executor`) would remain for mock, examples and tests.

Prerequisites:

- Decide whether depending on a `doc(hidden)` path is acceptable (not recommended).
- Check whether `score_crates` must change.
- Verify that the resulting graph builds and passes the relevant tests.

This is an optimisation separate from the decision below.

## 5. Decision

**Proposed decision, pending maintainer approval: retain `futures` for the current API.**

Rationale:

- `Subscription::to_stream()` exposes `futures::Stream`; replacing it is a breaking public API change.
- Production code uses only `Stream`, `AtomicWaker`, `Context` and `Poll`; executor use is limited to mock, examples and tests.
- Footprint reduction can be evaluated separately (section 4).
- Replacing `futures` is not recommended unless the stream API or runtime is intentionally redesigned.

This decision does not state that the crate or its integration is qualified or certified.

## 6. Safety Artifact Assessment

The presence of an artifact does not establish its completeness or acceptance for qualification. "Effort" uses relative sizes: **S** (small), **M** (medium), **L** (large).

### 6.1 Architecture and design documentation

**Available**
- `design/high_level_design_detail.md` (line 62 describes `Subscription` with async support), `architecture.puml`, `architecture_layers.puml`, `module_structure.puml`, `sequence_diagram/`.
- Code comments in `consumer.rs` describe the waker registration order and the discovery cleanup in `Drop`.

**Gaps**
- `futures` / `AtomicWaker` do not appear in the design documents (only in code comments).
- Progress, wake-up, cancellation and error-handling assumptions are not stated at design level.
- Rationale for choosing `futures` is only recorded in this document.

**Effort:** Small–Medium (extend design doc and diagram). **Status:** gap confirmed.

### 6.2 Requirements traceability

**Available**
- TRLC requirements under `score/mw/com/dependability/requirements/` (component and feature requirements), assumed-system requirement for Rust up to the highest ASIL (`assumed_system_requirements.trlc`, line 85).
- Component requirements for asynchronous `StartFindService` / `StopFindService` (C++ API).

**Gaps**
- A search of the dependability folder found no requirement for the Rust async stream or async receive behaviour (termination, cancellation, error items, exclusive access).
- No traceability from Rust async behaviour to design or tests.

**Approach:** add requirements for the observable Rust async behaviour only. Do not add a requirement that merely names an external crate.

**Effort:** Medium. **Status:** gap confirmed.

### 6.3 Unit test reports

**Available**
- Rust unit tests: `consumer.rs` (4 tests, lines 1224–1372), `producer.rs` (2), mock `runtime.rs` (3, one using `block_on`), `concept.rs`, `interface_macros.rs`, `sample_ptr.rs`, `sample_allocatee_ptr.rs`.
- `rust_test` target for the LoLa runtime; `quality/unit_testing/unit_testing.bzl` also provides QNX variants (`rust_test_qnx`).
- Integration binary `test/basic_rust_api/consumer_async_apis` and examples using `block_on` and tokio.

**Gaps**
- The 4 consumer unit tests cover info, subscribe, `try_receive` and type-ops error. None exercises `ReceiveFuture::poll`, `SampleStream::poll_next`, `ServiceDiscoveryFuture` or the wake-up path.
- `consumer_async_apis` builds the `rust_binary` `bigdata-consumer-async`, which is packaged in the same `BUILD` and driven by `integration_test/com_api_async_api_test.py`. Whether that integration test runs in CI and produces a result report was not checked and is **Open**.
- No requirement-to-test annotations for Rust async behaviour.

**Effort:** Medium (targeted tests for waker registration/wake, stream termination, error item, cancellation, discovery drop before callback). **Status:** gap confirmed.

### 6.4 Code coverage reports

**Available**
- LLVM-based coverage with Rust instrumentation including branch coverage (`quality/coverage/coverage.bazelrc`, Ferrocene toolchain).
- `quality/coverage/BUILD` includes `//score/mw/com/rust:score_com` in scope; justification file `coverage_justifications.yaml`.
- Coverage HTML and dashboard in `nightly_quality.yml` / `_coverage_report.yml`.

**Gaps**
- Current coverage numbers for `consumer.rs` poll paths are not recorded here. **Open:** run and attach the report.
- Coverage is expected to be low on the poll and wake paths given 6.3.
- Existing justifications are for C++ defensive code; none for Rust.
- QNX coverage is gcov-based and resets the Rust flags, so Rust branch coverage is Linux-only (stated in `coverage.bazelrc`).

**Effort:** Small (report) plus whatever tests in 6.3 are required. **Status:** infrastructure present, evidence open.

### 6.5 Static analysis results

**Available**
- Clippy over Rust targets with SARIF output, aggregated in the quality dashboard (`nightly_quality.yml`, `quality/dashboard/`).
- `rustc` warnings through `quality/compiler_warnings/`.
- ThreadSanitizer and AddressSanitizer workflows; `quality/sanitizer/tsan.supp` documents Rust runtime handling.

**Gaps**
- CodeQL / MISRA coverage is C++ only.
- Enabled Clippy lint set and review of its findings for async code are not documented. **Open.**
- No `unsafe` inventory or Miri run for the COM Rust crates.

**Effort:** Small (document tools, versions, lint set) to M (unsafe inventory, optional Miri). **Status:** partly available.

### 6.6 Qualification evidence

**Available**
- Version and SHA-256 pinning; Bazel-controlled build.
- Ferrocene Rust toolchain (qualified compiler) is used for build, coverage and sanitizer configurations.

**Gaps**
- No upstream safety or qualification documentation for `futures` is known; availability is **Open** and must be confirmed rather than assumed.
- No license text committed to this repository (the `futures-core` archive contains license files, see section 3 for the crates checked; `proc-macro2`, `quote`, `syn`, `unicode-ident` are not checked), no dated maintenance review and no advisory check.
- No documented assumptions about the crate (stability, `AtomicWaker` contract).

**Effort:** Medium (local evidence package); L if an independent review of upstream `unsafe` code is required. **Status:** gap confirmed.

### 6.7 Safety case documentation

**Available**
- FMEA records in `score/mw/com/dependability/safety_analysis/failure_modes.trlc` and FTA/mitigation structure.

**Gaps**
- A search for async, stream and waker terms found no Rust-async failure modes in the failure modes file.
- No stated assumptions about the external dependency and no mitigations for dependency-related failure modes (missed wake-up, stalled stream, upstream defect).
- No safety argument for retaining the crate.

**Approach:** extend the existing FMEA with async failure modes and add a short dependency argument; agree scope with the safety stakeholders.

**Effort:** Medium–Large. **Status:** gap confirmed.

### 6.8 CI/CD generated compliance artifacts

**Available**
- Workflows under `.github/workflows/`: host and QNX build/test, coverage, sanitizers, linter, CodeQL, nightly quality, flaky-test detection, dependency-version combination test, release and release post-processing, docs deployment, review checklists.
- `.github/CODEOWNERS`.
- Dashboard and release notes tooling.

**Gaps**
- No `cargo-audit`, `cargo-deny`, SBOM or license-report step found in the workflows, and no `deny.toml`.
- No automated dependency-update configuration (no Dependabot or Renovate file); crate versions change through `score_crates`.
- No documented change-control procedure for bumping the `futures` pin.

**Effort:** Small–Medium (advisory/licence check in CI, SBOM at release, written update procedure). Suitability of these tools for the Bazel-managed crate graph must be checked first. **Status:** gap confirmed.

### 6.9 Other work products

- Rust API documentation and user examples exist (`rust/doc/user_facing_api_examples.md`).
- Missing: consolidated third-party dependency record, and a requirement → design → test → coverage matrix for the Rust async path.

**Effort:** Small–Medium. **Status:** gap confirmed.

### Consolidated gap summary

| Area | Currently available | Gap | Effort |
|------|--------------------|-----|--------|
| Provenance / licensing | Version and SHA-256 pin; archive hashes and declared licenses/license files checked for the crates listed in Section 3 | Upstream source comparison, remaining dependency checks, license-text collection and Safety-team review | S-M |
| Maintenance | None stored | Dated upstream review | S–M |
| Architecture / design | General design docs and diagrams | `futures` contract and async assumptions | S–M |
| Requirements | C++ async find-service requirements | Rust async behaviour requirements | M |
| Unit tests | 4 consumer tests, mock and macro tests | Poll, wake, stream and discovery paths | M |
| Coverage | Rust LLVM coverage and dashboard | Current numbers, justifications | S |
| Static analysis | Clippy SARIF, sanitizers, compiler warnings | Lint-set documentation, `unsafe` inventory | S–M |
| Qualification evidence | Pinned Ferrocene toolchain, hash | Upstream evidence unknown, local evidence package | M–L |
| Safety case | FMEA structure | Async failure modes, dependency argument | M–L |
| CI compliance | Broad quality CI | Advisory, licence, SBOM, change control | S–M |

## 7. Qualification Feasibility Assessment

### Findings

- Production use is narrow: four items (`Stream`, `AtomicWaker`, `Context`, `Poll`), two of which are `std` re-exports. Only `Stream` and `AtomicWaker` carry upstream logic.
- The repository already provides much of the infrastructure that qualification needs: pinned toolchain, Rust coverage, Clippy, sanitizers, traceability tooling (TRLC) and a dashboard. Gaps are mainly content and evidence, not missing tooling.
- Nothing in the crate prevents producing these artifacts. The open risk is that upstream provides no safety evidence for `futures`; this needs confirmation and is independent of whether the crate is kept or forked.
- A fork would inherit the full qualification burden (upstream code review, maintenance, divergence) and would not remove the public API dependency on `futures::Stream` unless the API is changed.

### Approaches

| Approach | Description | Assessment |
|----------|-------------|------------|
| A. Retain upstream, qualified use | Fill the gaps in section 6, document the contracts COM relies on, verify them by tests and analysis. | Feasible; proportionate; preferred |
| B. A plus deeper dependency review | Additionally review upstream `unsafe` paths for `AtomicWaker` and `Stream`, if stakeholders require it. | Feasible; scope depends on required assurance level |
| C. Narrower dependency | Use `futures-core` and the `AtomicWaker` provider instead of the facade; `std::task` for `Context`/`Poll`. | Feasibility not confirmed (needs a `score_crates` configuration/build check); `futures-util::task::AtomicWaker` is public, while the `futures-core`-only variant needs a `doc(hidden)` path; could reduce the review surface; can combine with A |
| D. S-CORE-maintained fork or in-house waker/stream | Own the implementation. | Not justified now; revisit only for a concrete reason |

Reasons that would justify reconsidering D: upstream unmaintained, an unpatched critical defect, or stakeholder confirmation that upstream evidence is mandatory and unobtainable.

### Answer to the core question

The existing approach can be evolved to meet future qualification needs by continuing with upstream `futures` and strengthening artifact support (approach A, optionally with B and C). The required work is mostly documentation, requirements, tests and CI evidence. The acceptance criteria themselves still have to be agreed with the safety stakeholders.

## 8. Recommendation

**Continue with the existing upstream `futures` crate and enhance artifact support. Do not create or fork a dedicated crate at this time.**

Rationale:

1. The public API exposes `futures::Stream`; a replacement is a breaking change and a fork does not remove the dependency.
2. The used surface is small and COM-owned code is where most safety-relevant logic lives (waker registration order, FFI callback, `unsafe`).
3. Existing infrastructure already supports most artifact types.
4. A fork adds maintenance and qualification cost without a demonstrated need.

Conditions:

1. Add Rust async requirements and tests traced to them (6.2, 6.3).
2. Extend the FMEA and add a short dependency argument (6.7).
3. Record license, provenance and dated maintenance review (section 3).
4. Add advisory/license/SBOM checks and a version-update procedure (6.8).
5. Evaluate the footprint reduction (section 4) as a separate issue.
6. Agree required upstream evidence and acceptance criteria with safety stakeholders.

## 9. Next Steps

1. **Maintainer decision:** confirm the retain decision and whether footprint reduction becomes a separate issue (code owners in `.github/CODEOWNERS`).
2. **Evidence collection (author):** verify archive and license; record a dated upstream review; capture current Rust coverage, Clippy and sanitizer results; confirm that `consumer_async_apis` runs in CI.
   - Done: archive hashes and declared licenses for twelve crates, upstream tag commit, latest release, partial advisory check (sections 3, 11.1).
   - Required from maintainers: agree the scope (full closure or narrowed, 11.7 action 2) and confirm whether `consumer_async_apis` runs in CI.
3. **Safety stakeholders:** require a decision from the Safety team on which standards, evidence and acceptance criteria apply, and whether upstream evidence for `futures` is expected.
4. **Follow-up issues** (only for gaps accepted above): Rust async requirements; async unit tests; FMEA extension; design update; dependency governance in CI; footprint reduction.
5. **Share this record** with architecture and safety reviewers (acceptance criterion of #1263) and update the **Open** items once evidence is attached.

## 10. Acceptance Criteria Mapping and Review Record

| Acceptance criterion | Where addressed | Status |
|---|---|---|
| Current safety artifact coverage is documented | Section 6.1–6.9, particularly the **Available** subsections and consolidated gap summary | Done |
| Missing artifacts and qualification gaps are clearly identified | Section 6.1–6.9, particularly the **Gaps** subsections and consolidated gap summary | Done |
| Feasibility of achieving future safety qualification with the existing crate is evaluated | Section 7: Findings, Approaches, and Answer to the core question | Done |
| Recommendation and rationale for the preferred path forward are provided | Section 8: Recommendation, Rationale, and Conditions | Done |
| Findings are shared with stakeholders for architectural and safety review | Section 9, steps 3 and 5; review record below | Pending for review |

### 10.1 Review record

The criterion on stakeholder review is met when the reviewers below have reviewed this document (through the PR or the discussion in #1265) and the outcome is entered here.

| Role | Reviewer | Date | Outcome (approved / changes requested) |
|---|---|---|---|
| Maintainer / code owner | To be completed | To be completed | Pending |
| Architecture | To be completed | To be completed | Pending |
| Safety | To be completed | To be completed | Pending |

On approval, update the **Proposed decision** in section 5 to **Decision** and resolve or re-assign the **Open** items.

## 11. Crate-Level Classification

**Status: provisional, pending evidence. This is not a qualification or certification claim.** The outcome is only a proposal until the S-CORE Safety team reviews it. This section assesses `futures` itself, independent of the COM API (sections 1-10) and of the retain decision (section 5).

### 11.0 Method and sources

| Rule | Source |
|---|---|
| Record the qualification target first: crate name/version, repository and revision, enabled features, target platforms, dependency baseline | [`rust_crate_safety_guideline.rst`](https://github.com/eclipse-score/score-crates/blob/main/docs/rust_crate_safety_guideline.rst), Step 1 |
| Three tables: P (HE/PE/NE), C (NH/HM/NM), CLAS_OUT (Q/QR/NQ); the outcome is an initial proposal, final after Safety team review | S-CORE safety qualification guidelines (`score-crates: docs/rust_crate_safety_guideline.rst`), Steps 1 and 2 |
| External dependencies of the crate must also go through the process or be replaced | S-CORE safety qualification guidelines (`score-crates: docs/rust_crate_safety_guideline.rst`), Step 1 ("External dependencies") |
| Q/QR then address findings (coverage, docs, requirements, design, traceability); NQ cannot be used in the safety-critical codebase | S-CORE safety qualification guidelines (`score-crates: docs/rust_crate_safety_guideline.rst`), Steps 2-4 |
| P=1: no measure indicates PE/NE; P=2: some PE/NE but gaps acceptable (low or manageable risk); P=3 otherwise | S-CORE safety qualification guidelines (`score-crates: docs/rust_crate_safety_guideline.rst`), Step 1 |
| C=1: no HM/NM; C=2: some HM/NM but acceptable; C=3 otherwise | S-CORE safety qualification guidelines (`score-crates: docs/rust_crate_safety_guideline.rst`), Step 2 |
| Matrix: C=1 gives Q, Q, QR for P=1, 2, 3; C=2 gives QR for all P; C=3 gives QR, QR, NQ | S-CORE safety qualification guidelines (`score-crates: docs/rust_crate_safety_guideline.rst`), Step 3 |
| Activities: Q follows the qualification process; QR follows the pre-existing architectural element process; NQ not for safety context | S-CORE safety qualification guidelines (`score-crates: docs/rust_crate_safety_guideline.rst`), Step 5 |

No threshold for "high" LoC is defined. Whether the process classifies a whole crate, a sub-crate or a used subset is not stated explicitly; refer to the applicable S-CORE safety guidelines which speak of "the crate" with its enabled features and dependency baseline (see 11.6).

### 11.1 Qualification target record

| Item | Value | Evidence |
|---|---|---|
| Crate / version | `futures` 0.3.34 | `MODULE.bazel.lock` |
| Archive | `static.crates.io/crates/futures/0.3.34/download` | SHA-256 `9a31d2a3...e5d0a3` matches the lock (`sha256sum`) |
| Upstream revision | Tag `0.3.34` → commit `705e6b5c0f06535b1aac1cb1989a172b3d45be8c` | GitHub tag API returns this commit; the `futures` archive's `.cargo_vcs_info.json` records the same `sha1`. **Open:** source files at this commit not compared with the crates.io archive |
| Enabled features | `alloc`, `async-await`, `default`, `executor`, `futures-executor`, `std` | `MODULE.bazel.lock`; **Open:** feature sets of the other sub-crates |
| Target platforms | Linux and QNX builds exist in this repository | **Open:** confirm with architecture |
| Dependency baseline | `score_crates` 0.0.13 | `MODULE.bazel` (line 47) |

### 11.2 Features used

| Feature / item | Defined in (pinned source) | Used by | Scope |
|---|---|---|---|
| `Stream` | `futures-core` `src/stream.rs` | `Subscription::to_stream()` (public API), LoLa and mock runtime | Production |
| `AtomicWaker` | `futures-core` `src/task/__internal/atomic_waker.rs` (`doc(hidden)` module); re-exported by `futures-util` `src/task/mod.rs` | `consumer.rs` | Production |
| `Context`, `Poll` | `core::task` (re-exported by `futures-core`, `futures-task`, `futures-util`) | `consumer.rs` | Production |
| `executor::block_on` | `futures-executor` | mock test, example, `consumer_async_apis` | Non-production |
| `channel::oneshot` | `futures-channel` | example, `consumer_async_apis` | Non-production |
| `FutureExt`, `StreamExt`, `stream::empty` | `futures-util` | example, mock, tests | Non-production |

The facade enables more than production needs (`executor`, `futures-executor`); this comes from the shared `score_crates` configuration.

### 11.3 Current state in `score-crates`

- `docs/` in `score-crates` main (fetched) contains `safety_mgt`, `verification_report`, `rust_crate_safety_guideline.rst` and index files. There is no `futures` entry. Status: **not classified**.
- Expected layout: `component_classification.rst`, `requirement/`, `architecture/`, `safety_analysis/`, `component/`, `BUILD`. The guideline also names a `design/` directory, the shared `assumed_system_requirements.trlc` and `feature_requirements.trlc`, a per-crate `component_requirements.trlc`, AoU and failure modes.
- Known limitation: Rust test results are not shown in LOBSTER reports ([lobster#589](https://github.com/bmw-software-engineering/lobster/issues/589)).

### 11.4 Measurements (pinned archives)

Method: archives `https://static.crates.io/crates/<crate>/<crate>-0.3.34.crate` unpacked in `/tmp/fut`. LoC = non-blank lines in `src/**/*.rs` not starting with `//`, excluding `tests` directories. Unsafe = lines in `src/**/*.rs` containing the keyword `unsafe` that do not start with `//`; these are preliminary source-text measurements, not counts of compiled code, unsafe operations or unsafe blocks. Code disabled by conditional compilation (`cfg`) may be included, so the figures may overstate compiled code, while other effects of the counting method (for example `unsafe` in macros or generated code) are not assessed. Only `futures-core` was checked line by line. The LoUC / LoUC+N (safety-note) split required by the classification indicators is **incomplete** (not done).

| Crate | LoC | Source lines containing `unsafe` (preliminary) |
|---|---|---|
| `futures-core` | 306 | 5, checked line by line against the 0.3.34 source: `atomic_waker.rs` lines 273 and 385 are `unsafe` blocks, lines 422 and 423 are `unsafe impl Send/Sync` declarations, `stream.rs` line 232 is an `unsafe` block calling `map_unchecked_mut`. The script's 6th raw match (`lib.rs` line 11, the word `unsafe_op_in_unsafe_fn` inside a comment in a lint attribute) is not code and is excluded manually. |
| `futures-task` | 509 | 65 |
| `futures-util` | 16287 | 136 |
| `futures-channel` | 1334 | 23 |
| `futures-executor` | 599 | 10 |
| `futures-io` | 296 | 1 |
| `futures-sink` | 119 | 3 |
| `futures` (facade) | 94 | 1 |
| **Subtotal of listed futures workspace crates** | **19544** | **about 245 raw source-line matches, not verified unsafe operations (counts for crates other than `futures-core` not individually verified)** |

This subtotal covers the eight listed crate archives only. It is not a verified Bazel dependency-closure measurement and does not establish the complete third-party dependency scope.

**Lockfile-derived dependency inventory.** `MODULE.bazel.lock` identifies the eight listed `futures` workspace crates and the additional dependencies `futures-macro`, `memchr` 2.8.3, `slab` 0.4.12 and `pin-project-lite` 0.2.17; it lists `futures-util` features `async-await-macro`, `channel`, `io`, `sink`, `slab`, `memchr` and `std`. With network access, `bazel query 'kind(rust_.*, deps(@score_communication_crate_index//:futures))'` returned the same twelve crates plus `proc-macro2` 1.0.107, `quote` 1.0.47, `syn` 3.0.3 and `unicode-ident` 1.0.24 (dependencies of `futures-macro`, a proc macro) and rules_rust tooling targets. The enabled feature sets per crate were not extracted from the query, and `proc-macro2`, `quote`, `syn`, `unicode-ident` are not included in the measurements. Text-based counts (same method) for the three additional lockfile crates: `memchr` 8061 LoC / 342 lines containing `unsafe`; `slab` 752 / 6; `pin-project-lite` 1324 / 18; `futures-macro` 539 / 2. The eight-crate figures were reproduced on the same date, except that the raw `futures-core` match count is 6 (5 after the manual exclusion described above), so the subtotal of about 245 reflects the raw matches. Whether each crate is compiled for production targets (as opposed to build or host tooling) is **Open**.

Not measured: per-crate enabled features in the Bazel closure, the measurements of `proc-macro2`, `quote`, `syn`, `unicode-ident`, coverage, Clippy. The published archives contain no test files. The upstream CI workflow at tag `0.3.34` was read (see 11.5); upstream test sources and results were not inspected or run.

Reproduce the closure with `bazel query 'deps(@score_communication_crate_index//:futures)'` and review the resolved features in `MODULE.bazel.lock`.

### 11.5 Indicator assessment (provisional)

The upstream CI workflow (`.github/workflows/ci.yml` at tag `0.3.34`) and the `AtomicWaker` rustdoc in the pinned archive were read; issues, CI run results and the remaining rustdoc were not inspected. No indicator is rated. Every rating is **Not rated — evidence pending** until the Safety team has reviewed the evidence. The notes column records hypotheses only and is not a rating.

**P (process)**

| Id | Indicator | Evidence found | Missing | Rating | Notes (hypothesis, not a rating) | Follow-up |
|---|---|---|---|---|---|---|
| 1 | Rules and state-of-the-art processes for design, implementation, verification | Upstream `ci.yml` (tag `0.3.34`, runs on push to `main` and release branches and nightly): `cargo test --workspace --all-features` (debug and release), MSRV builds, `no-std` builds, feature powerset check, Miri (`cargo miri test`), sanitizer runs (address, memory, thread), minimal-versions build, tidy. The Clippy job is commented out with the note that the branch is no longer actively developed. No coverage, `cargo-audit` or `cargo-deny` job found. | CI run results; changelog; complete advisory review (section 3) | Not rated — evidence pending | CI definition is broad for tests and Miri; the CI file shows what is configured, not that it passes | Safety team to assess |
| 2 | Requirements available | None in S-CORE form; `AtomicWaker` rustdoc states a usage and memory-ordering contract (see section 2) | Requirements for `Stream` behaviour; rustdoc of `Stream` not reviewed | Not rated — evidence pending | Contract exists as rustdoc, not as requirements | Write TRLC requirements (pending: agreed scope and CLAS_OUT outcome) |
| 3 | Specification (architecture) | Source comments and `AtomicWaker` rustdoc describe the waker contract | Specification document; `Stream` rustdoc not reviewed | Not rated — evidence pending | Specification may be partial | Review `Stream` rustdoc |
| 4 | Design specification | None found | Static design for the used parts | Not rated — evidence pending | Design documents may be absent | Create design in `score-crates` (pending: depends on the agreed scope and the CLAS_OUT outcome) |
| 5 | Configuration specification | Cargo feature declarations only | Feature and platform record (11.1, partly done); whether other configuration data exists and is specified | Not rated — evidence pending | Feature declarations alone do not establish HE | Confirm indicator interpretation with Safety team (pending: Safety team decision) |
| 6 | Verification measures | No tests in archives; upstream CI defines tests, Miri and sanitizer jobs (see row 1) | Upstream test sources and results, coverage for the agreed scope | Not rated — evidence pending | None | Run upstream tests and coverage (pending: scope not agreed) |

P cannot be set yet. P=2 requires the Safety team to accept the identified gaps as low or manageable risk; that is not assumed here.

**C (complexity)**

| Id | Indicator | Measured eight-crate subtotal (partial, not full closure; see 11.4) | `futures-core` only | Rating |
|---|---|---|---|---|
| 1 | Lines of code | 19544 (11.4) | 306 | Not rated — evidence pending (threshold to be defined) |
| 2 | Unsafe code | about 245 source lines containing `unsafe` (eight-crate subtotal; preliminary text-match count, see 11.4) | 5 source lines containing `unsafe` (four in `AtomicWaker`, one unsafe block involving `map_unchecked_mut`; see 11.4) | Not rated — evidence pending (full unsafe analysis required) |
| 3 | Tests and coverage | Not measured | Not measured | Not rated — evidence pending |
| 4 | Public interfaces | Not counted | Not counted | Not rated — evidence pending |
| 5 | Parameters | Not counted | Not counted | Not rated — evidence pending |

Note: `futures` has custom atomic code and manual `Send`/`Sync` implementations in `AtomicWaker`, which requires careful safety analysis during qualification.

### 11.6 Classification outcome

**Provisional / pending evidence.** No final classification outcome is proposed at this stage because the P and C ratings have not been fully established against the classification indicators.

The measured source-line counts and preliminary unsafe-code counts indicate that the assessment scope and complexity require further investigation. However, these measurements do not by themselves establish C=2 or C=3. Complete the indicator assessments using the classification method, document the rationale for each rating, and obtain Safety team review before proposing a final CLAS_OUT.

The classification matrix (11.0) will be applied once P and C have been justified. The retain decision in Section 5 remains separate from this classification.

**Scope question.** The unit is the crate with its enabled features, and its external dependencies must also be classified or replaced. Reviewing only the APIs used does not shrink the classified scope unless the compiled dependency graph shrinks. Narrowing options: (a) depend on `futures-util` directly (about 16k lines, little reduction); (b) `futures-core` plus `std::task`, which reaches `AtomicWaker` only through the `doc(hidden)` `__internal` path and needs a `score_crates` change (`score-crates` `MODULE.bazel` on main declares only `futures` as a spec, so `futures-core` is not a direct target; not confirmed for 0.0.13); (c) an S-CORE-owned waker (not assessed). Feasibility under `score_crates` and Bazel was not tested; maintainers and the Safety team must confirm.

### 11.7 Actions

Required before classification:

1. `score-crates`, `docs/futures/docs/component_classification.rst`: write the report from 11.1, 11.4 and 11.5.
2. Maintainers and Safety team: agree the scope (full closure or narrowed) and whether the `__internal` path is acceptable.
3. Extract per-crate enabled features from the `bazel query` closure (crate list obtained, see 11.4) and decide which crates are compiled for production targets.
4. Review upstream CI run results, test sources, issues and the `Stream` rustdoc at `0.3.34` (CI definition and `AtomicWaker` rustdoc already read, see 11.5), and run upstream coverage for the agreed scope.
5. Count unsafe lines with and without safety notes for the agreed scope.

Subsequent qualification activities (subject to the applicable CLAS_OUT process and Safety team review):

6. `score-crates`: TRLC requirements (shared ASR and feature files, per-crate component requirements), AoU, failure modes (missed wake-up, stalled stream), design, traced tests, LOBSTER report.
7. Raise a change request so the module safety plan follows the CLAS_OUT activity.
8. Align `score_crates` feature set with the narrowed dependency scope only if Section 7, Approach C (narrower dependency) is selected.
