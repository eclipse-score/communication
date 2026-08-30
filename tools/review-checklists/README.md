<!--
*******************************************************************************
Copyright (c) 2026 Contributors to the Eclipse Foundation

See the NOTICE file(s) distributed with this work for additional
information regarding copyright ownership.

This program and the accompanying materials are made available under the
terms of the Apache License Version 2.0 which is available at
https://www.apache.org/licenses/LICENSE-2.0

SPDX-License-Identifier: Apache-2.0
*******************************************************************************
-->

# Review Checklists

A GitHub Actions composite action (plus a pair of workflows) that posts
per-path review checklists on pull requests, tracks reviewer
acknowledgements as threaded "OK" replies, and gates merging on every
approving reviewer having acknowledged every checklist relevant to the
files they're approving.

It also understands GitHub's merge queue: when the queue runs its checks
(`merge_group`) it re-validates the originating PR's checklist evidence
from scratch rather than assuming it is still valid, and while a PR is
enqueued it posts a notice on any other checklist-relevant event
(reviewed, commented on, edited) explaining that the evidence block in
the PR description is a live, ever-updating view — the authoritative,
tamper-evident record of what evidence existed is the merge commit's
message in git history, not this description.

## How it works

For every path pattern group ("checklist") whose `include`/`exclude` glob
patterns match at least one changed file in the PR, the action:

1. Posts a file-level PR review comment (a "finding") containing the
   checklist body, anchored to one of the matched files. Reviewers
   acknowledge a checklist by replying to that specific conversation
   thread with exactly `OK` (case-insensitive).
2. Tracks, for each checklist, which of the PR's current approving
   reviewers have replied `OK` in that checklist's thread.
3. Sets a commit status (context `review-checklists`) to `success` only
   once every current approving reviewer has acknowledged every relevant
   checklist; otherwise `pending` (or `failure`).
4. Invalidates (deletes) all existing `OK` replies for a checklist when
   new commits touching its paths are pushed, so approvals must be
   re-acknowledged against the new code.
5. Maintains a Markdown "evidence" block in the PR description that
   records the checklist/acknowledgement state, and a merge-queue notice
   (comment + PR description block) while the PR is enqueued.

### Two-stage workflow (trigger / apply)

This logic needs to run — and be able to write PR comments and commit
statuses — for events that can be raised by a fork PR
(`pull_request_review`, `pull_request_review_comment`). GitHub always
downgrades `GITHUB_TOKEN` to read-only for those events, no matter what a
workflow's `permissions:` block requests. To get a writable token safely,
the logic is split into two workflows following GitHub's
["preventing pwn requests"](https://securitylab.github.com/resources/github-actions-preventing-pwn-requests/)
pattern:

- **`review_checklists_trigger.yml`** (unprivileged, `permissions: {}`):
  reacts to every event that can affect checklist state
  (`pull_request_target`, `pull_request_review_comment`,
  `pull_request_review`, `merge_group`). It performs no checkout and
  executes no repository/PR-supplied code. The only data transferred to
  the second workflow is the PR number, read directly from the trusted
  event payload (`github.event.pull_request.number`, or parsed from the
  merge queue's synthetic ref for `merge_group`) and uploaded as a build
  artifact. This is safe to trust as-is — it's never attacker-influenced
  code or a value that steers which logic runs, and at worst a wrong
  number would just point the second workflow at the wrong already-public,
  same-repository PR. Everything else the second workflow needs is derived
  natively from the `workflow_run` context, the GitHub API, or a live diff
  against prior run history.
- **`review_checklists_apply.yml`** (privileged, triggered via
  `workflow_run` once the trigger workflow completes): `workflow_run`
  always executes using the *base* repository's workflow file and
  permissions, regardless of which repository (including forks) raised
  the original event. This is what lets it safely hold a
  `pull-requests: write` / `statuses: write` token. It downloads the PR
  number artifact uploaded by the trigger stage, derives the original
  event name and head SHA from `github.event.workflow_run`, and invokes
  this composite action with the appropriate `action` input.

Because the apply workflow checks out the **base branch** (`main`), not
the PR branch, the checklist logic itself (and `.github/review_checklists.yml`)
always runs as defined on the base branch — a PR cannot alter or disable
its own checklist enforcement by editing files on its own branch.

## Layout

```
tools/review-checklists/
├── action.yml              # Composite action: action=post|check|dismiss_sync
├── requirements.txt(.in)    # Locked Python dependencies (pip-compile via Bazel)
├── BUILD                    # Bazel target to (re-)generate requirements.txt
└── scripts/
    ├── helpers.py           # Shared GitHub API / checklist-matching helpers
    ├── post_checklists.py   # action=post   – post/update checklist findings
    ├── check_acknowledgements.py  # action=check – verify acks, set commit status
    ├── dismiss_and_invalidate.py  # action=dismiss_sync – invalidate stale OKs
    └── *_test.py            # pytest unit tests for the above (run via Bazel)
```

The two workflow files that drive this action live in
`.github/workflows/review_checklists_trigger.yml` and
`.github/workflows/review_checklists_apply.yml`.

## Setting this up in a repository

**This is not a turnkey drop-in — the following must be configured for
each repository that wants to use it.**

1. **Vendor the code.** Copy `tools/review-checklists/` and both
   `.github/workflows/review_checklists_trigger.yml` /
   `review_checklists_apply.yml` into the target repository (or point the
   `uses:` steps in the apply workflow at this repository/ref instead of
   the local `./tools/review-checklists` path, e.g.
   `eclipse-score/communication/tools/review-checklists@<ref>`, if you'd
   rather reference it remotely than vendor it).
2. **Create `.github/review_checklists.yml`** in the target repository
   with at least one checklist entry (see [Configuration](#configuration)
   below). An empty `checklists: []` is valid and simply means no
   checklist ever applies — the workflows still run but are a no-op.
3. **Adjust the base branch filter.** `review_checklists_trigger.yml`
   restricts `pull_request_target` to `branches: [main]`. Change this if
   your default branch has a different name.
4. **Require the commit status, not a workflow check run, in branch
   protection.** Because the apply workflow only ever runs via
   `workflow_run`, GitHub cannot bind it as a "required workflow" check
   directly on the PR. Instead, configure your branch protection rule /
   ruleset to require the commit status context **`review-checklists`**
   (the one this action sets via `set_commit_status`). Requiring the
   workflow job itself will not gate merges correctly. If you manage
   branch protection with [Otterdog](https://otterdog.readthedocs.io/en/latest/reference/organization/repository/status-check/),
   a plain commit status (as opposed to a status reported by a GitHub
   Actions workflow job or a GitHub App) must be referenced with the
   `any:` prefix, i.e. `any:review-checklists` — plain `review-checklists`
   will not be recognized.
5. **Require at least one approving review** in branch protection. The
   check only evaluates *approving* reviewers against acknowledgements —
   with zero approvals, the commit status stays `pending` forever by
   design (see `check_acknowledgements.py`), but you still need a real PR
   review requirement configured for that to have teeth.
6. **If you use GitHub's merge queue**, be aware the merge-queue evidence
   notice assumes the queue's merge method is `MERGE` (a real merge
   commit) — the evidence block accumulated in the PR description is only
   preserved for reviewers/auditors if it ends up in a merge commit's
   message/history. If your merge queue is configured for `SQUASH` or
   `REBASE`, the notice's premise ("post-queue changes do not alter
   evidence recorded in git history by the merge commit") does not hold,
   and this should be reconsidered. You can check your repository's
   configured merge method under the ruleset's `merge_queue` rule
   (`merge_method`).

   Even with `MERGE` selected, GitHub does **not** default to embedding the
   PR title/description into the merge commit message — by default the
   merge commit message is a generic "Merge pull request #N from ..."
   line, which does not capture the evidence block at all. You must also
   configure the repository so the merge commit message is built from the
   PR title and body:
   - `merge_commit_title: "PR_TITLE"`
   - `merge_commit_message: "PR_BODY"`
7. **Reviewers must acknowledge by replying `OK`** (exact text,
   case-insensitive) directly in the threaded conversation under the
   bot-posted checklist comment — a general PR approval alone does not
   count as an acknowledgement of any checklist.
8. **Findings are posted as file-level review comments** (`subject_type:
   "file"`) anchored to the first matched file, not to a specific diff
   line/position. This means `include` patterns may freely match binary
   files (images, archives, etc.) or any other file GitHub does not
   render a text diff for — posting the finding does not depend on the
   file having a diff hunk.
9. **Runner requirements.** The workflows run on `ubuntu-24.04` GitHub-hosted
   runners, use the pre-installed `gh` CLI and `python3`/`pip`, and install
   this action's Python dependencies from PyPI on every run (outbound
   network access is required; nothing is vendored/cached at runtime). If
   you use self-hosted runners, ensure `gh`, `python3`, and PyPI access are
   available.
10. **Keep the trigger workflow's filename as `review_checklists_trigger.yml`**,
    or update `TRIGGER_WORKFLOW_FILE` in `scripts/dismiss_and_invalidate.py`
    to match — it's looked up by filename via the GitHub API to find the
    previous trigger run for a branch (used to compute which files changed
    since the last push without needing any before/after SHA handed across
    the trust boundary).

## Configuration

`.github/review_checklists.yml` (path is also overridable via the
composite action's `config-path` input, but is read from the **base**
branch, not the PR branch — see above):

```yaml
checklists:
  - id: example-review          # Unique identifier (used in markers/tracking)
    name: "Example checklist"   # Human-readable name shown in the PR conversation
    include:                    # Glob patterns; a changed file must match at least one
      - "**"
    exclude:                    # Optional: glob patterns; matching files are excluded
      - ".github/**"
    checklist: |                # Markdown checklist body shown to reviewers
      - This is an example checklist item
      - Avoid checkmarks in the items — makes it easy to accidentally
        modify the checklist
      - Modifying the checklist resets all previous acknowledgements
```

Notes:
- The file must exist and contain a `checklists` key; `checklists: []`
  means no checklist ever applies (the workflows still run but do nothing).
- A file matching multiple checklists triggers all of them.
- Changing a checklist's `checklist:` body text does not retroactively
  invalidate prior acknowledgements of that checklist — only new pushes
  touching its `include` paths do (see `dismiss_and_invalidate.py`).
- **`include`/`exclude` patterns use `.gitignore`-style syntax** (via the
  [`pathspec`](https://pypi.org/project/pathspec/) library's `gitwildmatch`
  pattern style), the same syntax used in `.gitignore` files:
  - A pattern without a leading `/` matches at any depth: `"*.md"` matches
    both `NOTE.md` and `docs/deep/NOTE.md`.
  - A pattern with a leading `/` is anchored to the repo root: `"/*.md"`
    matches only root-level `.md` files, not `docs/NOTE.md`.
  - `**` explicitly matches zero or more path segments: `"docs/**"` matches
    everything under `docs/`; `"**/BUILD"` matches a `BUILD` file at any
    depth, including the repo root.
  - `"**"` on its own matches everything (used by the example checklist
    above).

## Composite action inputs (`action.yml`)

| Input | Required | Used by | Purpose |
|---|---|---|---|
| `action` | yes | all | One of `post`, `check`, `dismiss_sync` |
| `github-token` | yes | all | Token with `pull-requests: write` / `statuses: write` |
| `pr-number` | no | all | Pull request number (for `merge_group` events, the originating PR whose evidence is validated) |
| `run-id` | no | `dismiss_sync` | Current trigger-workflow run id (for run-history lookup) |
| `head-branch` | no | `dismiss_sync` | Head branch name (scopes the run-history lookup) |
| `head-sha` | no | `check` | Commit SHA to set status on for `merge_group` events |
| `event-name` | no | `check` | Overrides ambient event name when invoked from `workflow_run` |
| `config-path` | no | all | Path to the checklist YAML (default `.github/review_checklists.yml`) |

## Development

Python dependencies are locked with Bazel/`rules_python` pip integration:

```sh
# Run all tests
bazel test //tools/review-checklists/...

# Regenerate requirements.txt after editing requirements.txt.in
bazel run //tools/review-checklists:requirements.update
```

At workflow runtime, dependencies are installed directly via
`pip install -r requirements.txt` — Bazel is only used for local
development/testing and lock-file generation, not by the GitHub Actions
runner.
