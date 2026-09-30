# Feature and cycle records

Default location: `<subsystem>/work/<feature>/`. For repository-wide work, use
`docs/engineering/work/<feature>/`. Keep feature names semantic and stable across
branches. Do not create a shared registry or rewrite other features when adding one.

```text
<feature>/
  README.md
  knowledge-candidates.md       # only when there are candidates
  cycles/
    01-<topic>.md
    02-<topic>.md
```

The README records the overall objective and boundaries and points to the current
cycle. It can index older cycles by topic, but their records own their status and
acceptance. A cycle can span sessions; a feature can span branches and accepted
cycles. Numbering is local to a feature. Reuse an existing equivalent structure when
it preserves these distinctions; do not create empty files to satisfy the diagram.

## Feature README

```markdown
# <Feature>

Objective: <semantic outcome>
Boundary: <included and excluded work>
Current cycle: [<topic>](cycles/01-<topic>.md)

## Context
<Links to relevant depots and sources; cross-feature dependencies if any.
Describe what completes the whole feature, beyond the current cycle.>
```

## One record per cycle

```markdown
# <Cycle>

Status: proposed | in progress | ready for review | accepted | paused | blocked
Baseline: <revision; affected scope/platform; relevant dependency version>
Request and scope: <human request; bounded result; exclusions>
Decision gates: <unsettled decisions needing human input; authorization already given>

## Current conclusions and impact
<Distinguish authored contract, agreed intent, observed behavior, and hypotheses.>

| Artifact / ID | Semantic change or re-pin | Old → new | Dependents / validation |
|---|---|---|---|

## Evidence
| Command or review | Configuration / inputs | Result | Findings / exclusions |
|---|---|---|---|

## Investigation and decisions
<Concise entries for consequential experiments, rejected approaches and reasons,
human decisions and their source. Correct conclusions above when evidence changes;
preserve useful reasoning here without appending every step or command.>

## Handoff
<Next action; unresolved questions; pause reason and resumption condition if any;
knowledge candidates to consolidate.>

Acceptance: <not given, or explicit human acceptance and source>
```

Omit inapplicable sections; a read-only cycle need not invent an artifact/version
table. Use `not run`, `blocked`, and `not applicable` accurately. Record command
success separately from findings. Development warnings may be permitted. Keep a
short current handoff even when history grows; move substantial evidence to a
supporting file in this cycle's feature directory when needed.

## Staged knowledge

In `knowledge-candidates.md`, group proposed amendments by destination topic. For
each retain the proposed delta, source/reproduction, confidence (observed, agreed
intent, or hypothesis), and the unresolved decision if any. Do not copy whole depots
as editable snapshots. Other features are not required to load these candidates.

Knowledge required to interpret changed product artifacts must be updated in its
shared home with those changes. Staging is for incidental discoveries, uncertainty,
and conflict-prone improvements, not permission to leave necessary guidance wrong.
At consolidation, reread current shared knowledge and relevant sources, discard
superseded candidates, and apply only the remaining supported changes.

## Retirement

Keep accepted cycles while their feature needs them. Before removing a completed
feature, consolidate durable facts/rationale, transfer residual obligations, and
check inbound references and cross-feature dependencies. Preserve recoverable
history at a Git revision or evidence-store location before deletion; uncommitted
files are not preserved by Git. Remove only this feature within authorized cleanup
scope. Never delete another active/paused feature or treat a merged branch as proof
of feature completion. No permanent completion log is required in shared depots.
