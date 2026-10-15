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

## 1. Pinned Version and Features

| Item | Value | Source |
|------|-------|--------|
| Version | `0.3.34` | `MODULE.bazel.lock` (`crate_index__futures-0.3.34`) |
| Archive | `https://static.crates.io/crates/futures/0.3.34/download` | `MODULE.bazel.lock` |
| SHA-256 | `9a31d2a3fbaaeb2af2368bbdd904aa8e812d3c04a1ee10d3171f52d556e5d0a3` | `MODULE.bazel.lock` |
| Enabled features | `alloc`, `async-await`, `default`, `executor`, `futures-executor`, `std` | `MODULE.bazel.lock` (`rust_library` target for `futures`) |
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

- `ReceiveFuture::poll`, `SampleStream::poll_next` and `ServiceDiscoveryFuture::poll` call `register(cx.waker())` **before** attempting to receive or reading shared state. The code comments state this is to avoid a missed wake-up against the FFI callback.
- The C++ discovery callback calls `wake()` after storing handles under a `Mutex`.

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
| Provenance | Pinned by version and SHA-256 in `MODULE.bazel.lock` (hash re-checked against the lock). Fetched from `static.crates.io`. | Partly verified; **Open:** independent archive verification |
| Licensing | Expected `MIT OR Apache-2.0` (compatible with Apache-2.0 used by S-CORE). No license text is stored in this repository. | **Open:** confirm in pinned archive |
| Maintenance | Upstream is `rust-lang/futures-rs`. No dated review is stored. | **Open:** dated release/activity/advisory review (also confirm that `0.3.34` is the intended release) |
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

The public `Stream` return type keeps a `futures` ecosystem dependency. The production footprint could shrink by depending only on the needed `futures-*` sub-crates (the trait is defined in `futures-core`; the sub-crate providing `AtomicWaker` must be confirmed) and using `std::task` for `Context` and `Poll`. The full `futures` crate (with `executor`) would remain for mock, examples and tests.

Prerequisites:

- Confirm which sub-crate exports `AtomicWaker` and `Stream`.
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
- Rust unit tests: `consumer.rs` (4 tests, lines 1225–1372), `producer.rs` (2), mock `runtime.rs` (3, one using `block_on`), `concept.rs`, `interface_macros.rs`, `sample_ptr.rs`, `sample_allocatee_ptr.rs`.
- `rust_test` target for the LoLa runtime; `quality/unit_testing/unit_testing.bzl` also provides QNX variants (`rust_test_qnx`).
- Integration binary `test/basic_rust_api/consumer_async_apis` and examples using `block_on` and tokio.

**Gaps**
- The 4 consumer unit tests cover info, subscribe, `try_receive` and type-ops error. None exercises `ReceiveFuture::poll`, `SampleStream::poll_next`, `ServiceDiscoveryFuture` or the wake-up path.
- `consumer_async_apis` is a `rust_binary`; whether it runs as a test target and produces a result report is **Open**.
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
- No stored license text, dated maintenance review or advisory check.
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
| Provenance / licensing | Version + SHA-256 pin | License text, archive verification | S |
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
| C. Narrower dependency | Use `futures-core` and the `AtomicWaker` provider instead of the facade; `std::task` for `Context`/`Poll`. | Feasible; reduces the review surface; can combine with A |
| D. S-CORE-maintained fork or in-house waker/stream | Own the implementation. | Not justified now; revisit only for a concrete reason |

Reasons that would justify reconsidering D: upstream unmaintained, an unpatched critical defect, or stakeholder confirmation that upstream evidence is mandatory and unobtai-nable.

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
2. **Evidence collection:** verify archive and license; record a dated upstream review; capture current Rust coverage, Clippy and sanitizer results; confirm that `consumer_async_apis` runs in CI.
3. **Safety stakeholders:** ask which standards, evidence and acceptance criteria apply, and whether upstream evidence for `futures` Ais expected.
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
