# PR "ball-in-court" overview design

## Problem

Pull requests from external contributors against `eclipse-score/communication`
stall because it is unclear whose turn it is: maintainers forget to review, and
contributors go silent after feedback. The existing `stale_pr.yml` ignores whose
turn it is and therefore treats a PR waiting on maintainers like one waiting on
its author.

## Goal

At any time, a maintainer or contributor can see with one click which PRs
require an action from us and which require an action from the PR author.

## Non-goals

- No GitHub Project board (can be layered on the labels later).
- No auto-merge.
- No changes to required checks or the review-checklist workflow.

## State model

Every open, non-draft PR carries exactly one `state:*` label. First matching
rule wins.

| Label | Rule |
|---|---|
| `state:needs-author` | Merge conflicts; a required check failing; changes requested and no later author action (DCO/ECA are covered by required checks) |
| `state:ready-to-merge` | Approved by a code owner, required checks green, no conflicts, review checklists satisfied |
| `state:needs-maintainer` | Everything else: no review yet, author pushed or commented after the last review, a review was re-requested, CI green and awaiting review |
| none | Draft PRs (label removed when converted to draft) |

Notes:

- "CI green" considers required checks only; nightly and flaky jobs are ignored.
- Bot PRs (Dependabot, `peter-evans/create-pull-request`) are included.
- Time in state is derived from the label's `labeled` timeline event, so no
  extra storage is needed.

## Components

### `pr_state_trigger.yml` (unprivileged)

Triggers: `pull_request_target`, `pull_request_review`,
`pull_request_review_comment`, `issue_comment`. Performs no checkout and no write. Uploads the PR
number as an artifact. Same trust-boundary pattern as
`review_checklists_trigger.yml`, so fork PRs (read-only token) work.

### `pr_state_apply.yml` (privileged)

Triggered by `workflow_run` of the trigger workflow, plus a hourly `schedule`
sweep (covers missed events such as check results) and `workflow_dispatch`. Runs the state script for
one PR (event) or all open PRs (sweep). Needs `pull-requests: write`. Never checks out
PR code; the script is taken from the base branch.

### State script

`tools/pr-state/pr_state.py` (Python, standard library plus `gh` CLI).

- A pure function `compute_state(pr_snapshot) -> label` that has no I/O.
- A thin layer fetching the snapshot through GraphQL (reviews, review
  decisions, mergeable state, required-check rollup, last push/comment times,
  draft flag) and swapping labels.
- Unit tests for `compute_state` covering each rule and rule precedence.
- Labels are created idempotently (`gh label create --force`).

### Stale policy (`stale_pr.yml`, modified)

- `state:needs-author` and unlabelled PRs: marked `stale` after 14 days, closed 7 days later (the
  current behaviour, implemented via `exempt-pr-labels` for the other two states, so drafts keep their
  behaviour).
- `state:needs-maintainer`: never auto-closed. After 7 days in the state the
  digest highlights it, and a `needs-maintainer-overdue` label is added.
- `stale` is removed when the state flips (any author activity).

### Digest

A weekday cron job (`pr_digest.yml`) updates one pinned issue, "PR overview",
with two tables: `needs-maintainer` sorted by age (oldest first), and
`needs-author` PRs within 3 days of going stale. Editing a single issue avoids
notification spam; an optional Slack webhook secret can be added later.

## Error handling

- API or rate-limit errors fail the job loudly; the daily sweep self-heals.
- Concurrency group per PR number prevents racing label swaps.
- If the script cannot determine the state, it leaves labels unchanged.

## Testing

- Unit tests of `compute_state`.
- `workflow_dispatch` with a `dry_run` input that logs intended label changes.
- Workflows only log intended changes until the repository variable
  `PR_STATE_ENABLED` is `true`; roll out in dry-run for one week first.

## Rollout

1. Merge script, tests and workflows in dry-run mode.
2. Enable writes; create labels.
3. Modify `stale_pr.yml` and add the digest.
4. Update `CI.md` (see below).

## CI design update

`CI.md` gets new entries under "Periodic 24h" / event-driven jobs:
"PR state labelling", "Stale pull requests" and "PR overview digest".
