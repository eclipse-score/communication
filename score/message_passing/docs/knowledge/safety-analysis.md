# Safety Analysis and Open Judgments

Read when reviewing failure modes, FTA causes, measures, assessments, or AoUs.
Scope: QNX ASIL-B transport argument; tooling 3.0.2, inspected 2026-11-04.
This guidance does not establish a completed or certified safety argument.

## Reason Before Recording

Use the [safety-analysis skill](../../../../.github/skills/score-safety-analysis/SKILL.md)
for the current workflow. Derive failure behavior from the actual QNX `.cpp`
path and relevant tests, not only headers, design prose, or Linux code. Confirm
uncertain severity, applicability, containment, and measure sufficiency with the
safety reviewer before writing formal records.

Separate the caller-visible effect, actionable root cause, actual implemented
mechanism, environmental obligation, and argument for sufficiency. A syntactically
connected tree can still be semantically wrong. A capped reconnect retry is not
an independent server health check merely because an old leaf label says so.

## Current Traceability Model

The pinned schema and rules now use:

```text
public_api diagram -> generated ApiMethod <- FailureMode.interface
FailureMode <- FTA RootCause <- RootCauseAssessment
RootCauseAssessment -> versioned CompReq / own AoU measures
```

- `architectural_design.package` names the generated public-API TRLC package.
  The current package is `message_passing_api`. Reference actual generated
  `ApiMethod` records, not old method-name strings or guessed flattened IDs.
- `failure_modes` needs the architectural design in `deps` when referencing
  its generated API records. `FailureMode.interface` is a typed optional list;
  omitting it does not demonstrate API coverage.
- `fault_trees` generates `RootCause` records from `$RootCause` leaves. Do not
  hand-author generated stubs or apply old `$BasicEvent` name-matching rules.
- Each root cause is assessed exactly once. An assessment lists versioned
  measures and a sufficiency rationale. Empty `measures` means a reasoned
  non-applicability argument, not "analysis still to do".
- A `CompReq` is an ordinary component requirement; it does not derive from a
  root cause. An own AoU has no root-cause field or `derived_from`.
- List measure targets in `safety_analysis.safety_measures`. Listing an AoU
  target identifies its records as own AoUs for export/forwarding; coverage by
  assessments is a separate traceability obligation. An unused file is neither
  a wired own AoU nor an established safety argument.
- Assessment ASIL must cover its failure modes and cannot exceed its measures'
  ASIL. Refer to the current schema, not older Mitigation/ControlMeasure models.

[The safety BUILD](../../dependability/safety_analysis/BUILD) owns the current
rules and FTA list. [Assessments](../../dependability/safety_analysis/assessments.trlc),
[failure modes](../../dependability/safety_analysis/failure_modes.trlc), and
[safety-measure requirements](../../dependability/safety_analysis/safetymeasures.trlc)
are distinct authored inputs.

## Baseline and Review Boundary

Treat completeness as an explicit review outcome, not a development-mode build
result. Consult [safety-analysis completeness](issues/safety-analysis-completeness.md)
for current gaps and evidence. Do not fill gaps with invented safeguards or make
an incidental knowledge edit into a safety rewrite. A skipped manual check is
not acceptance.

## Intent That Still Needs Review

For containment and reporting policy changes, read
[failure-containment intent](decisions/2026-11-04-failure-containment-intent.md).
Check its approval status and actual per-path evidence; do not infer one uniform
fail-stop policy from recovered discussion.

An integrator obligation and a partial internal mitigation can coexist. For
example, lack of a timing guarantee does not erase real buffering/dispatch
behavior, but buffering alone does not close an end-to-end timing obligation.
Keep that reasoning explicit in the assessment rather than choosing an AoU or
an implementation measure solely to make the tree mechanically complete.

Peer misuse versus same-process contract misuse also need distinct arguments.
Do not assume an empty required callback is a harmless no-op. Confirm the
actual callback invocation path and dependency behavior when that matters.

Identity reporting and trustworthy identity/environment configuration are
separate responsibilities; see [identity responsibilities](issues/requirements-client-identity-aous.md).

## Verification

Use [the explicit checks](build-and-test.md#explicit-seooc-checks-and-reports)
for safety analysis and the DE, rather than a wildcard that skips manual tests.
Inspect generated API/FTA records, the assessment links, requirement allocation,
and test traces. Rendering an FTA or resolving its aliases is not proof that
its causes or safety judgments are correct. New command results must record
the tooling version and actual findings, not reuse historical totals.