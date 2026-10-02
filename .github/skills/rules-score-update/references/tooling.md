# SEooC update mechanics and evidence

Reusable reasoning for `rules_score` projects. Exact types, attributes, validator
behavior, and commands are version-dependent: confirm them from the resolved
dependency. Repo-specific pins belong in repository guidance; version-specific external
constraints and workarounds belong in the external tooling depot, not universal rules here.

## Four kinds of evidence

| Kind | What it establishes | What it does not establish |
|---|---|---|
| Authored requirement/design | The currently expressed obligation/model | Correct implementation or achieved qualification |
| Recorded human decision | Intended behavior, scope, or an agreed exception | That all artifacts were updated or the intent is implemented |
| Source/test inspection | Observed path, configuration, or exercised case | A broader guarantee across all platforms/failure paths |
| Tool result | The checks actually executed over included inputs | Semantic completeness, coverage of omitted inputs, or approval |

Keep disagreements explicit. The baseline is the starting contract, not infallible
ground truth. A later approved decision can require reconciling it; copied memory
is only evidence of that decision until provenance and current status are checked.

## Preflight the source and build graph

Read the repository module/configuration and resolve `@score_tooling` to the selected
version or override. Skills copied from the tooling repository may contain paths
that do not exist in a consumer checkout. Use the external source tree actually
used by the build; a similarly named vendored RSL is not automatically equivalent.
If unavailable, record the access gap and avoid claiming schema/tool verification.

For affected inputs, establish this chain:

```text
source file → owning rule and attribute → dependency closure → validator/test → result
```

Check `srcs`, `deps`, architecture diagram categories, platform compatibility, manual
tags, coverage allocation, and any coverage lock. Distinguish absent lock/target,
omitted input, skipped incompatible target, and genuinely passing validation. Discover
generated targets using the pinned rules or a Bazel query when the tool is available;
do not guess suffixes from another version.

## Requirement and safety reasoning

Choose requirement level by the scope and observer of the obligation. Establish
capability/constraint at the appropriate parent level before enumerating methods.
Every derivation should narrow or realize a parent's meaning, not just reference an
existing record. Check platform conditions and safety allocation explicitly.

For an FTA/FMEA change, first inspect the pinned schema's permitted roots and links.
The reviewed workflow reports formal FTAs rooted in `FailureMode` and control/AoU
relationships governed by that model. An informal system-level tree is not automatically
valid input to the same rules. Do not invent a type or field to force it into the graph.
An optional conceptual method must remain opt-in and must not alter the schema.

For each candidate cause, identify the real failure point, propagation, detection,
control, residual effect, and integration obligation. Inspect both shared and applicable
platform code, including ignored return values and missing callbacks. Distinguish
established behavior, design intent, and unresolved disposition. Record partial
mitigation without exaggerating it into a guarantee. Names that sound like mechanisms
are not evidence of mechanisms.

## Trace and version policy

Before edits, enumerate dependent references across requirements, FTA expressions,
test annotations, coverage locks where present, and textual/diagram relationships.
Repeat the focused search after edits to catch omissions. A lexical scan is useful
for identifiers and pins but cannot replace the schema or semantic review.

Use the [repository's version convention](../../../../docs/engineering/seooc-maintenance.md#local-version-convention).
A pure same-parent re-pin still requires checking the child's meaning. Changes to
meaning or parent identity extend the impact; diagrams without schema versions do
not acquire invented version fields. Before retirement, record disposition and
confirm remaining references are reconciled. Do not regenerate a coverage lock to
conceal drift; review its change and retain coverage intent.

## Validation dimensions

1. Schema/type/reference checks over all affected files, including explicit handling
   of files currently outside the build graph.
2. Semantic review of requirement refinement, code behavior and platform/safety scope.
3. Architecture naming/interface/method checks over correctly categorized inputs.
4. Actual diagram/document rendering and warning inspection; parsing and rendering
   are separate pipelines and may disagree.
5. Relevant implementation and integration tests on the applicable configurations.
6. Requirement/test trace and coverage-drift checks where configured. Explain any
   missing allocation or unvalidated platform.

Select the applicable dimensions from the impact, not a fixed full-repo command list.
Capture baseline findings to distinguish new issues. Development-mode warnings are
permitted; inspect affected generated documentation and explain the remaining
misalignment without expanding the cycle to fix every warning. Schema validation
and version-link checking are different dimensions. Functional tests supply behavior
evidence; Lobster traces relate it to obligations. Treat unsupported schema syntax
as an unverified parser/toolchain mismatch, not a permitted modeling warning.

If a workaround disables a check, retain the exact scope, observed tool version if
known, reproduction evidence, compensating review, and a condition for reevaluation
after upgrade. Prefer a real fix; do not spread a bypass as a generic style rule.
