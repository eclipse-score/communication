# Maintaining Message-Passing Knowledge

Read when adding, reviewing, or setting up access to this developer knowledge.
Scope: `score/message_passing`; this is guidance, not a normative SEooC artefact.

## Reading and Authority

Start with [the component's agent entry point](../../AGENTS.md) and read only
the topic relevant to the task. Follow individual decision or issue links when
they affect that task; do not preload the whole knowledge directory.

Keep these kinds of evidence distinct:

- Requirements and API contracts state intended obligations.
- Implementation and tests establish observed behavior and verification coverage.
- Decisions explain reviewed intent; they do not silently change a requirement.
- Issues describe gaps, hypotheses, and unresolved judgments, not guarantees.
- The pinned tooling source determines schema and validator behavior. Skills and
  historical observations can lag that version.

When sources disagree, record the discrepancy and agree the intended change with
the relevant reviewer. Do not rewrite a contract merely to match the code, or
invent a safety mechanism to make a diagram appear complete.

Use the [repository contribution guide](../../../../CONTRIBUTING.md) for general
development conventions. Keep existing design prose in
[the client/server design](../client-server.md), rather than duplicating it here.

## Choosing a Home

- Put stable, task-specific orientation in the owning topic page.
- Put one independently reviewable decision in `decisions/YYYY-MM-DD-slug.md`.
- Put one independently resolvable finding or question in `issues/area-slug.md`.
- Extend an existing record for the same concern instead of creating duplicates.
- Use descriptive filenames, not a shared sequence counter or a developer name.
- Do not add a session diary, central append-only backlog, or empty placeholders.

Topic pages should contain a short "Read when", scope, current model or workflow,
verification instructions, and selective evidence/record links. Prefer roughly
100-150 lines, but do not pad small topics or split them into trivial files.
Keep detailed explanations and mutable state in one authoritative home. Short,
linked reminders of critical constraints and task-local validation commands are
useful; do not duplicate evolving status or evidence snapshots.

Link new records beside the relevant concern in the owning topic. Prefer neutral
link labels so status changes stay in the record. Split findings when their
status, owner, or next check can change independently.

## Evidence and Freshness

Use relative links to existing source files, with symbols or Bazel labels in the
surrounding text. Prefer these over copied signatures or fragile line numbers.
Use a commit or PR link when the historical state is essential to the argument.
Do not depend on private memory, personal checkout paths, caches, or temporary
rendering output.

For version-sensitive observations, record the inspected date and tooling
version, and distinguish source inspection from a reproduced command result.
State the working directory, prerequisites, and expected result of a command.
Never label an old finding count or workaround as newly verified.

Keep implementation support, build-tooling readiness, and analysis coverage
separate. In particular, a temporary platform restriction on a SEooC wrapper
does not imply that the underlying implementation lacks that platform support.
Revisit temporary workarounds when their upstream dependency changes.

## Decision Template

```markdown
# Descriptive Decision Title

Status: proposed | accepted | superseded
Date: YYYY-MM-DD
Scope: affected API, backend, or process

## Context
Problem and constraints, with evidence links.

## Decision
The chosen approach, rationale, and relevant alternatives.

## Consequences
Effects and limits; link related issues where needed.

## Evidence
Review/PR or explicit owner decision, plus supporting source links.
Supersedes / superseded by: link if applicable.
```

Only mark a decision accepted when approval evidence exists. Recovered intent
without that evidence remains proposed or becomes an issue for confirmation.
The initial migration must not promote old discussion into a new safety claim.

## Issue Template

```markdown
# Descriptive Finding Title

Status: open | blocked | deferred | resolved | needs-revalidation
Area: functionality | build | tooling | requirements | architecture | safety
Scope: backend, affected behavior, and relevant tooling version
Owner: reviewer role, or explicitly unassigned
Evidence checked: YYYY-MM-DD; source inspection or command reproduction

## Finding
Expected versus observed behavior and impact; distinguish unknowns from facts.

## Evidence
Source links, symbols, and reproduction/prerequisites when available.

## Next Check
Smallest discriminating check, upstream dependency, or owner question.
Resolution: PR/commit and outcome when resolved.
```

Do not inherit old task priorities or assign people without agreement. Resolve a
record in the PR that fixes its cause. Update a topic page only when orientation,
contracts, routing, or retained reminders change; status/evidence-only updates
can stay in the record. Git and PRs retain the history; the agent entry point
need not change for each new issue or decision.

## Pull Request Checklist

- [ ] Update affected knowledge with the behavior, contract, or tooling change.
- [ ] Distinguish current facts, reviewed intent, and unverified findings.
- [ ] Check relative links and remove stale or machine-specific references.
- [ ] Recheck version-sensitive notes when the tooling pin changes.
- [ ] Include verification evidence without claiming skipped checks passed.
- [ ] Review cross-domain effects with functionality and SEooC reviewers.
- [ ] Keep changes local; do not fix unrelated code or safety artefacts here.

## Agent Discovery

Discovery and linked-file loading differ by harness; a Markdown link alone does
not guarantee inclusion or token savings. See the
[VS Code instruction reference](https://code.visualstudio.com/docs/agents/reference/custom-instructions-reference).

For native Copilot, check nested agent instructions along the paths being
worked on. VS Code Local requires `chat.useAgentsMdFile` and
`chat.useNestedAgentsMdFiles` for nested discovery; these do not configure other
harnesses. For Codex, start in the component directory or explicitly load its
entry point when the session starts at repository root.

After setup, use a fresh session for a small component task. Inspect instruction
references/debug logs and actual file reads: the entry point and relevant topic
should load, without unrelated topics. Also check a task outside the component.
Attach the entry point explicitly if discovery is unavailable. Do not create
competing copies or edit personal workspace settings as part of this layout.