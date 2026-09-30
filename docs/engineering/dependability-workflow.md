# Working on dependability with agents

This document explains the workflow introduced on 2026-09-26 and the reasons for
its structure. It is written for developers familiar with this repository, Bazel,
TRLC, PlantUML, and the dependability tooling.

**This is a human-maintained design explanation, not a routine agent input.**
It may become outdated as the workflow evolves. Agents must not edit it without
an explicit human request to author or revise this document. Ordinary feature work,
knowledge consolidation, or a tooling upgrade does not authorize refreshing it.
An agent asked to revise the workflow may consult it for background, then verify
its assumptions against current skills and sources.

## Two kinds of repository memory

We need a small amount of current knowledge that survives individual features, and
enough working history to resume a feature without reconstructing a conversation.

The knowledge depots hold the first kind. They explain current facts, decisions and
their reasons, source locations, useful commands, and known discrepancies. Humans
and agents both maintain them. Old explanations and resolved issues can be removed:
their history remains in Git.

Feature directories hold the second kind. They contain the objective, current work
state, investigation history, proposals, and human decisions for one semantic
feature. Agents do most of this record keeping, but the files remain reviewable and
editable by humans. A feature directory can survive multiple commits and branches.
It is removed when the feature is complete and its enduring knowledge has been
consolidated.

This separation prevents a shared scratchpad from becoming both an increasingly
large session transcript and an unreliable description of the current subsystem.

## Knowledge follows scope

| Scope | Initial location | Typical content |
|---|---|---|
| External context | [tooling-context.md](tooling-context.md) | Schema constraints, validator behavior, copied-skill limitations |
| Repository | [seooc-maintenance.md](seooc-maintenance.md) | Dependency selection, documentation commands, QNX configuration, local conventions |
| Subsystem | [message-passing maintenance](../../score/message_passing/maintenance/README.md) | Boundary, design intent, architecture facts, known discrepancies, validation wiring |

These scopes identify where knowledge belongs; they are not a precedence hierarchy.
A subsystem decision cannot override a schema constraint. Code can contradict an
authored requirement without making that requirement automatically wrong. A human
decision can establish intended behavior without proving the implementation already
provides it. The depots preserve such distinctions and point to the evidence needed
to reconcile them.

The tooling repository and copied `score-*` skills remain upstream-owned.
Our `rules-score*` skills coordinate their use locally. We can document limitations
and recommend upstream improvements without silently maintaining local forks.

Depots are deliberately selective. A complete inventory of requirements or API
methods would mostly duplicate maintained sources and become stale. Useful knowledge
is information that changes the next engineer's decisions: for example, that the
client implementation is shared while server implementations are platform-specific,
or that `--config=qnx` is available and what a successful cross-build establishes.

## Features contain cycles

A feature is the overall semantic objective. A cycle is a bounded increment reviewed
by a human. A session is simply one period of work with an agent.

These boundaries need not coincide. An interrupted session leaves its cycle open.
An accepted cycle may finish one stable part of a feature while further cycles
remain. A merged branch does not necessarily finish the semantic feature.

The default structure is:

```text
score/message_passing/
  maintenance/
    README.md
    decisions.md
    open-items.md
    validation.md
  work/
    <feature>/
      README.md
      knowledge-candidates.md
      cycles/
        01-<topic>.md
        02-<topic>.md
```

The feature README states the objective, boundaries, and current-cycle pointer.
The cycle record owns its scope, current conclusions, consequential history,
validation evidence, outstanding questions, and acceptance. We do not maintain
separate copies of that status in the README, a next-steps file, and a shared backlog.
Supporting files are added when useful, rather than requiring the same five-file
bundle for every cycle.

Cycles retain the useful part of the previous workflow. The earlier API review,
requirement refinement, and diagram reconciliation were meaningful review increments.
The problem was not those boundaries; it was duplicated state, ambiguous completion,
and accumulated logs. A concise record should explain why an approach was rejected
or a guarantee changed without recording every command or routine step transition.

## Human decisions and agent continuity

At the start of a cycle, the human and agent establish its intended result and
decision boundaries. The request itself may already settle these. The agent uses
that authorization for routine work instead of repeatedly asking permission for
mechanical changes.

Uncertain guarantees, safety allocation, incompatible behavior, and material scope
expansion still need human decisions. The agent prepares evidence and concrete
alternatives before asking. At the end of a cycle it reports the result for
acceptance; completed edits and passing checks do not imply acceptance.

A review request remains a review. Finding an implementation problem is not an
instruction to fix it. A paused cycle records its reason and resumption condition,
and is not resumed just because an agent discovers it.

On resumption, the agent reads the relevant depot, the feature README, the current
cycle, and applicable staged knowledge. It consults older cycles for specific
questions rather than reloading the full history.

## Shared knowledge on parallel branches

Different developers may change the same subsystem while working on separate
features. Small topic-based documents and narrow edits reduce conflicts. Stable
headings are preferable to regularly regenerated indexes, global progress tables,
timestamps, and append-only shared changelogs. A topic can be split when size or
contention makes that useful; one file per fact is unnecessary.

Knowledge that is needed to understand a changed artifact is updated with that
artifact. Incidental discoveries, uncertain conclusions, and broad reorganizations
can instead wait in the feature's `knowledge-candidates.md`. Each candidate records
the proposed change, its intended destination, and evidence. It is not a copied
snapshot of the shared depot or an automatic override.

Candidates are consolidated when the feature finishes or the human asks. The agent
first compares them with current sources and shared knowledge, because another
feature may already have changed the relevant facts. Promotion applies the remaining
supported delta and preserves other work.

This arrangement limits contention without requiring a special Git workflow.
Branching, rebasing, merging, and PR management remain manual and outside the skills.

## Validation is evidence about particular claims

Development maturity allows dependability checks to report warnings. The workflow
does not demand a clean release-level model before every useful increment, nor
require unrelated findings to be repaired in the current cycle.

Instead, the agent inspects the affected generated documentation and reports,
separates existing findings from introduced ones, and explains relevant gaps.
`bazel build //docs/sphinx:sphinx_doc` produces the documentation used for this
review. A successful command can coexist with misaligned traceability shown there.

Schema validation, reference-version checking, diagram consistency, rendering,
functional tests, requirement tracing, and coverage-lock checking are distinct.
The cycle chooses the evidence relevant to its claims. QNX functional tests can be
built using `--config=qnx`; cross-compilation is not runtime evidence, and functional
success is not complete dependability traceability. A workaround that bypasses a
validator check needs an explicit account of what remains unchecked.

## Completion and independent retirement

Multiple feature directories can coexist. Completing one does not alter or remove
another, including a paused feature.

Before retiring a directory, consolidate enduring facts and decision rationale,
transfer unresolved obligations to an appropriate maintained home, and resolve any
cross-feature dependencies. Shared knowledge and product artifacts must not depend
on temporary feature files remaining present. Git or an evidence store must contain
recoverable history before deletion; an uncommitted file has no such protection.

There is no permanent feature-completion ledger in the depots. The current knowledge
remains, while investigation history can be retrieved from Git when needed.

## How the initial adoption is packaged

[rules-score](../../.github/skills/rules-score/SKILL.md) holds shared lifecycle and
knowledge rules plus bootstrap guidance.
[rules-score-update](../../.github/skills/rules-score-update/SKILL.md) adds incremental
impact and cascade mechanics. Both use the same feature/cycle record conventions.
The component prompt now routes to current knowledge rather than recreating a
baseline inventory on each invocation.

The initial depots were extracted from existing message-passing material and checked
against current sources and available tooling. Paused FTA reasoning has a current
feature handoff; it is not resumed or accepted by this migration. Existing research
and copied memory are retained as historical input, outside routine loading.
Their later removal is separate cleanup, not a prerequisite for using this workflow.

Future workflow changes should start from the active skills and repository behavior.
This document preserves the design reasoning, including the choices to retain cycles,
stage conflict-prone knowledge, and keep upstream ownership explicit. It is not a
second operational specification to keep synchronized automatically.
