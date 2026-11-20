..
   # *******************************************************************************
   # Copyright (c) 2026 Contributors to the Eclipse Foundation
   #
   # See the NOTICE file(s) distributed with this work for additional
   # information regarding copyright ownership.
   #
   # This program and the accompanying materials are made available under the
   # terms of the Apache License Version 2.0 which is available at
   # https://www.apache.org/licenses/LICENSE-2.0
   #
   # SPDX-License-Identifier: Apache-2.0
   # *******************************************************************************

Futures Crate Component Classification
======================================

.. document:: Futures Crate Component Classification
   :id: doc__futures_crate_comp_class
   :version: 1
   :status: draft
   :safety: ASIL_B
   :security: NO
   :realizes: wp__sw_component_class
   :tags: pre-existing, software-component, classification, Rust, async


| Classification of Futures Crate as a software component in the context of S-CORE.
|
| Repository: https://github.com/rust-lang/futures-rs
| Documentation: https://docs.rs/futures/0.3.34/futures/
| Crate Version: 0.3.34 (resolved in ``Cargo.lock`` of score_crates v0.0.13, which this repository uses)
| Additional documentation considered:
| https://crates.io/crates/futures


Component used from the futures crate
-------------------------------------

Only the following items are used in production code of the Rust COM API. Everything else (``executor::block_on``, ``channel::oneshot``, ``FutureExt``, ``StreamExt``) is used in mock, example and test code only and is out of scope. The classified component is therefore ``futures-core``.

.. list-table:: Used items
    :header-rows: 1

    * - Item
      - Defined in
      - Used for
    * - ``Stream``
      - ``futures-core`` (``src/stream.rs``)
      - Return type of the public ``Subscription::to_stream()``
    * - ``AtomicWaker``
      - ``futures-core`` (``src/task/__internal/atomic_waker.rs``), re-exported as ``futures::task::AtomicWaker``
      - Wake-up of pending receive and discovery futures
    * - ``Context``, ``Poll``
      - ``core::task``, re-exported by ``futures``
      - No ``futures`` logic; provided by the Rust ``core`` library


Step 1: Determine (P): the uncertainty of the Processes applied
---------------------------------------------------------------

| Apply the process measures to determine (P).
| The result of a process measure shall have as outcome [HE, PE, NE]
| - HE: High Evidence
| - PE: Partly Evidence but Manageable
| - NE: No Evidence

The evidence below comes from the futures dependency review (``futures_dependency_review.md``, section 11.5) and from the upstream repository at tag ``0.3.34``. The ratings are proposed and are final only after review by the S-CORE Safety team.

.. list-table:: Determine (P)
    :header-rows: 1

    * - Id
      - Indicator for applying process
      - Result
      - Rationale for result

    * - 1
      - Are rules, state-of-the art processes applied for the design, implementation and verification?
      - PE (proposed)
      - The project follows standard Rust open-source practices (checked against upstream ``.github/workflows/ci.yml`` at tag ``0.3.34``; it runs on pull requests, on push to ``main`` and the release branches, and nightly).

        CI verifications include:

        * test suite (``cargo test --workspace --all-features``, debug and release) on Rust nightly, on Linux (x86_64 and arm64), macOS and Windows, and cross-targets armv7, armv5te, i686 and s390x
        * build checks on stable, beta and nightly
        * Code Coverage: none. No coverage job in ``ci.yml`` (tag ``0.3.34``, branches ``0.3`` and ``main``), no coverage tool configuration (for example Codecov, Coveralls, ``cargo-llvm-cov``, ``tarpaulin``) in the tag's file tree, no coverage badge in the README, and no coverage threshold for pull-request approval was found. Upstream issues with "coverage" in the title: none found.
        * MSRV checks (1.36 for ``futures-core``, ``futures-io``, ``futures-sink``; 1.71 for the other crates)
        * ``no-std`` builds (thumbv7m, thumbv6m), feature powerset check and minimal-versions build
        * Documentation generation (``docs`` job)
        * Clippy linting: the job is commented out upstream, because the branch is no longer actively developed (``RUSTFLAGS: -D warnings`` is set for the other jobs)
        * Miri UB detection
        * address, memory and thread sanitizer runs (``cargo test`` on Rust stable)
        * formatting check (``cargo fmt --check``) and GitHub Actions workflow audit (``zizmor``)
        * dependency staleness checks: Dependabot (cargo weekly, github-actions monthly)
        * CHANGELOG entries in ``CHANGELOG.md`` for each release (0.3.34 and 0.3.33 listed with pull-request numbers)
        * GitHub issues are used (issues enabled, 283 open on 2026-11-16)

        No ``cargo-audit`` or ``cargo-deny`` job found.

        The ``futures`` project consists of several crates, so detailed information is in the crate documentation on docs.rs and in the source and rustdoc comments.

        * crate documentation: https://docs.rs/futures/0.3.34/futures/
        * documentation of the used component: https://docs.rs/futures-core/0.3.34/futures_core/
        * source of the used component (tag ``0.3.34``): https://github.com/rust-lang/futures-rs/tree/0.3.34/futures-core/src

        CI results: the run of this workflow on the ``0.3.34`` tag commit (``705e6b5c``, branch ``0.3``, 2026-08-11, run 31490059232) completed with all 24 jobs successful (tests, builds, MSRV, ``no-std``, feature powerset, minimal-versions, Miri, the three sanitizers, bench, docs and tidy). The latest run on branch ``0.3`` (``7aba6860``, 2026-11-02) also succeeded. The ``main`` branch CI is not used here because it differs (it has an active Clippy job).

        Missing: the complete advisory review.

    * - 2
      - Are requirements available?
      - PE (proposed)
      - The functional requirements of the used items are partially captured in the crate documentation (rustdoc) as expected behaviors, with usage guidance for each. No requirements exist in S-CORE form.

        * ``Stream`` (``futures-core`` ``src/stream.rs``): ``poll_next`` returns ``Poll::Pending`` when the next value is not ready and the implementation ensures the current task is notified when it may be ready; ``Poll::Ready(Some(val))`` yields a value; ``Poll::Ready(None)`` means the stream has terminated and ``poll_next`` should not be called again. Polling after termination may panic or block, but must never cause undefined behavior. ``size_hint`` gives bounds on the remaining length; it must not be trusted to omit bounds checks in unsafe code, and the default is ``(0, None)``.
        * ``AtomicWaker`` (``futures-core`` ``src/task/__internal/atomic_waker.rs``): consumers call ``register`` before checking the result and producers call ``wake`` after producing it; ``wake`` before ``register`` is a no-op; one ``AtomicWaker`` may be reused for any number of calls. Memory ordering: ``register`` acquires all memory released by earlier ``wake`` calls; for concurrent ``register`` calls (to be avoided) ordering is guaranteed only for the winning call. An example (``Flag``) shows the required check after ``register`` to avoid lost notifications.

        (crate documentation - https://docs.rs/futures-core/0.3.34/futures_core/stream/trait.Stream.html and https://docs.rs/futures/0.3.34/futures/task/struct.AtomicWaker.html)

        Missing: requirements in S-CORE form (TRLC) for the behavior used by the COM API; ``AtomicWaker`` is in a ``doc(hidden)`` module of ``futures-core`` and is documented on docs.rs through the ``futures::task`` re-export.

    * - 3
      - Are specifications for functionalities and properties available (architecture)?
      - PE (proposed)
      - The crate documentation partially specifies the functionality and properties of the used items, through the trait and type descriptions, return-state descriptions and a usage example (checked in ``futures-core`` 0.3.34):

        * ``Stream``: the trait is described as the asynchronous version of ``Iterator``, modeled after ``Future``; the ``poll_next`` return states, the behavior after termination and the ``size_hint`` properties are specified (see Id 2).
        * ``AtomicWaker``: the purpose, the ``register``/``wake`` calling pattern and the memory-ordering guarantees are specified (see Id 2). Comments in the source describe the implementation, which uses a single ``AtomicUsize`` with ``REGISTERING`` and ``WAKING`` bits, the single-producer behavior and the outcome of concurrent ``register`` calls.
        * The ``futures-core`` crate has a README (usage and minimum Rust version 1.36).

        The specification is partial: there is no separate specification document.

        Missing: specification in S-CORE form.

    * - 4
      - Are design specifications available?
      - PE (proposed; may be judged NE, as no design document exists)
      - No UML or class diagram, no architecture or design document in the upstream repository (no ``docs`` or design directory in the 0.3.34 tree). The design of the used component is documented only in source comments, mainly the description of the ``AtomicWaker`` state handling (see Id 3). The used component is small (``futures-core``, 306 lines, see Step 2), but its unsafe code needs a design description.

        Missing: static design for the used parts (``Stream``, ``AtomicWaker``); to be created in score-crates.

    * - 5
      - Are configuration specification and data available, if applicable?
      - HE (proposed)
      - Not applicable.

        The used component (``futures-core``) does not use any runtime configuration, environment-based settings or configuration files. So no configuration specification is applicable or needed. Only compile-time Cargo features (``std``, ``alloc``, optional ``portable-atomic``) select the compiled code.

    * - 6
      - Are verification measures including tests and reports available?
      - PE (proposed)
      - Test suite exists in the upstream repository, with test files covering unit tests and integration tests (62 test files with 235 ``#[test]`` attributes in ``futures/tests``, including ``task_atomic_waker.rs`` and 13 ``stream*`` test files; further tests in ``futures-channel`` and ``futures-executor``). The ``futures-core`` crate has no tests of its own, and the published archives contain no test files.
        (test suite - https://github.com/rust-lang/futures-rs/tree/0.3.34/futures/tests)
        (test suite result is available from the CI run on the ``0.3.34`` tag commit, all 24 jobs passed - https://github.com/rust-lang/futures-rs/actions/runs/31490059232, or by running tests locally with ``cargo test --workspace --all-features`` command)

        No code coverage report is generated or published upstream: the pull-request checks contain no coverage job (e.g. - https://github.com/rust-lang/futures-rs/pull/3069/checks), and neither does the CI run on the ``0.3.34`` tag commit. Coverage of the used component is not measured (see Step 2, Id 3).

        Missing: coverage of ``futures-core``; ``AtomicWaker`` is covered by one test only.


| (P=1) shall be selected when none of the determined process measures indicate PE or NE.
| (P=2) shall be selected when at least one of the determined process measures indicate PE or NE, but the gaps evaluated are acceptable, means
|       the risk of systematic faults due to these gaps is sufficiently low or manageable by mitigating the gaps.
| (P=3) in all other cases.
|

Futures crate is proposed as **P = 2** (to be confirmed by the Safety team).


Step 2: Determine (C): the uncertainty of finding systematic faults based on the Complexity
-------------------------------------------------------------------------------------------

| Apply the complexity measures to determine (C).
| The result of a complexity measure shall have as outcome [NH, HM, NM]
| - NH: Not High
| - HM: High but Manageable
| - NM: high and Not Manageable

The numbers below are measured on the used component ``futures-core`` 0.3.34. The ratings are proposed and are final only after review by the S-CORE Safety team. Lines of code are non-blank lines in ``src/**/*.rs`` not starting with ``//``.

.. list-table:: Determine (C)
    :header-rows: 1

    * - Id
      - Indicator for high Complexity
      - Complexity measure Tool
      - Result
      - Number

    * - 1
      - High amount of Lines of Code
      - Lines of Code (without comments) (generated code is excluded, e.g. ProtoCmpl)
      - NH (proposed; threshold for "high" is not defined)
      - 306

    * - 2
      - Unsafe code used / total unsafe code
      - Count:
            * LoUC+N: lines of unsafe code with safety note
            * LoUC  : lines of unsafe code, no safety note
      - HM (proposed; full unsafe analysis required)
      - 5 source lines containing ``unsafe``, checked line by line (``atomic_waker.rs``: two ``unsafe`` blocks and ``unsafe impl Send/Sync``; ``stream.rs``: one ``unsafe`` block calling ``map_unchecked_mut``)

        The split into LoUC and LoUC+N is not done.

    * - 3
      - | Test exists / Coverage (Function, Line)
        | (maybe better: testability, but how to measure?)
      - Existing Tests Coverage
      - HM (proposed; coverage not measured, so NH cannot be claimed)
      - Not measured. The published archives contain no tests. Upstream CI defines tests, Miri and sanitizers but no coverage job, and publishes no coverage metrics.

    * - 4
      - High amount of public function interfaces
      - Number of public function interfaces
      - NH (proposed)
      - 6 for the used items in ``futures-core`` 0.3.34: ``Stream::poll_next``, ``Stream::size_hint`` (default method), ``AtomicWaker::new``, ``AtomicWaker::register``, ``AtomicWaker::wake``, ``AtomicWaker::take``. The COM API calls ``new``, ``register`` and ``wake``, and implements ``poll_next``. (Other public items of ``futures-core``, such as ``FusedStream`` and ``TryStream``, are not used and not counted.)

    * - 5
      - High amount of function parameters
      - Number of parameters
      - NH (proposed)
      - At most 2 (``poll_next(self: Pin<&mut Self>, cx: &mut Context<'_>)``; ``register(&self, waker: &Waker)``); ``new`` has none, ``size_hint``, ``wake`` and ``take`` only take ``&self``.

Note: ``futures`` has custom atomic code and manual ``Send``/``Sync`` implementations in ``AtomicWaker``, which requires careful safety analysis during qualification.

| (C=1) shall be selected when none of the determined complexity measures indicate HM or NM.
| (C=2) shall be selected when at least one of the determined complexity measures indicate HM or NM, but the gaps evaluated are acceptable, means
|       the risk of systematic faults due to these gaps is sufficiently low in the context of S-CORE or manageable by mitigating the gaps.
| (C=3) in all other cases.
|

Futures crate is proposed as **C = 2** (to be confirmed by the Safety team).


Step 3: Determine (CLAS_OUT): the classification outcome
--------------------------------------------------------

| Select CLAS_OUT depending on the determined values of (C) and (P)

.. list-table::
   :header-rows: 1
   :widths: 20 20 20 20

   * -
     - P=1
     - P=2
     - P=3

   * - C=1
     - Q
     - Q
     - QR

   * - C=2
     - QR
     - QR
     - QR

   * - C=3
     - QR
     - QR
     - NQ


Futures crate is proposed as **CLAS_OUT = QR**, as it has **P = 2** and **C = 2** (to be confirmed by the Safety team).


Step 4: Document all results and rationale for choosing (P) and (C) and (CLAS_OUT)
----------------------------------------------------------------------------------

| Results (proposed, to be confirmed by the Safety team):
| - P = 2 (Partly Evidence but Manageable): indicators 1-4 and 6 show PE and indicator 5 is HE. The gaps are acceptable and manageable, because the used component (``futures-core``) is small and its upstream CI runs tests, Miri and sanitizers, which passed on 0.3.34. The missing coverage, requirements and design can be added in score-crates.
| - C = 2 (High but Manageable): lines of code, public interfaces and parameters are NH. The unsafe code in ``AtomicWaker`` (custom atomics, manual ``Send``/``Sync``) is HM, and coverage is not measured.
| - CLAS_OUT = QR, from P = 2 and C = 2 according to the classification criteria.
|
| This classification is a proposal and not a qualification or certification claim.


Step 5: Based on (CLAS_OUT) select the activities
-------------------------------------------------

Futures crate is proposed as **CLAS_OUT = QR** (to be confirmed by the Safety team).

| As soon as the change request containing this is in status "Accepted", the module safety plan for the component development is adapted based on the following: (select according to above result)
| - Q: Follow the processes for qualification of software components in a safety context.
| - QR: Follow the process for pre-existing software architectural elements
| - NQ: Do no use this element in safety context
|

Additional notes:

Traceability:
End-to-end traceability is not yet established for the futures crate: score-crates (main) has no ``futures`` entry, so no requirements, design, failure modes, control measures or traced tests exist yet. They can be generated in the same way as for ``pastey``: a ``docs/futures/docs/`` folder in score-crates with ``component_classification.rst`` (this document), ``requirement/`` (assumed system, feature and component requirements in TRLC), ``architecture/`` (static design and public API in PlantUML), ``safety_analysis/`` (AoU and failure modes, for example missed wake-up and stalled stream), ``component/`` and ``tests/``, wired in a ``BUILD`` file as a ``dependable_element`` with integrity level B (``rules_score`` from ``score_tooling``). Traceability is then reported with the LOBSTER tool; unit test results are not currently displayed in the generated reports due to a known limitation in Rust test result parsing (https://github.com/bmw-software-engineering/lobster/issues/589).

Toolchain Assurance:
In this repository the crate is built with the Ferrocene toolchain, which is maintained as part of the score_toolchains_rust repository (``.bazelrc``, Linux and QNX configurations). The upstream tests run with the standard Rust toolchains (nightly, stable, beta and MSRV), not with Ferrocene; running the tests with the Ferrocene toolchain is open.

Use of Standard Library Components in futures-core:
The ``futures-core`` crate is ``#![no_std]``. The used items use only Rust core library components (``core::task`` for ``Context``, ``Poll`` and ``Waker``, ``core::pin``, ``core::cell::UnsafeCell`` and ``core::sync::atomic``). With the enabled ``alloc`` and ``std`` features, ``alloc`` is used for the ``BoxStream`` and ``BoxFuture`` type aliases and ``std`` only for the ``Stream`` and ``FusedFuture`` implementations for ``std::panic::AssertUnwindSafe``; these are not used by the COM API. Since they are sourced from the same Ferrocene toolchain, they use the same underlying components.

Open items before the change request is accepted:

1. Maintainers and Safety team: confirm that the classified component is ``futures-core`` and whether using ``AtomicWaker`` through the ``doc(hidden)`` ``__internal`` path is acceptable.
2. Add a coverage job in score-crates and measure coverage of ``futures-core``.
3. Count unsafe lines with and without safety notes (LoUC+N, LoUC) in ``futures-core``.
4. Safety team review of the proposed ratings and CLAS_OUT.

Assessment reference document (template used) : https://eclipse-score.github.io/process_description/main/process_areas/safety_management/guidance/template_component_classification.html
