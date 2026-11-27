# PR state labelling

Labels every open, non-draft pull request with whose turn it is, so that
maintainers and contributors get a work queue each:

| Label | Meaning |
|---|---|
| `state:needs-author` | Merge conflict, failing required check, or changes requested without a later author action |
| `state:needs-maintainer` | Awaiting a review or a reaction to new author activity |
| `state:ready-to-merge` | Approved and all required checks green |
| `needs-maintainer-overdue` | In `state:needs-maintainer` for more than 7 days |

Saved searches: `is:pr is:open label:state:needs-maintainer`,
`is:pr is:open label:state:needs-author`.

Author activity is a push, a comment, a review-thread reply, a review or a
review re-request by the author. Required checks are those of the branch
protection; other checks are ignored. A required check that is still
pending (e.g. the `review-checklists` status) prevents `state:ready-to-merge`.

## Files

* `pr_state.py` - `compute_state` is a pure function; the rest talks to the
  `gh` CLI. `python3 pr_state.py --repo OWNER/NAME [--pr N] [--dry-run]`
* `pr_digest.py` - updates the pinned issue "PR overview".
* `pr_state_test.py` - unit tests, `bazel test //tools/pr-state:pr_state_test`.
* Workflows: `pr_state_trigger.yml`, `pr_state_apply.yml`, `pr_digest.yml`,
  `stale_pr.yml`. See [CI.md](../../CI.md).

## Enabling

Workflows only log intended changes (dry run) until the repository variable
`PR_STATE_ENABLED` is set to `true`.
