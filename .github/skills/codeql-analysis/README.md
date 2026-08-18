# codeql-analysis skill

Run, reproduce, and debug the repository's CodeQL / MISRA static analysis
(`//quality/static_analysis:codeql_lint`) without relearning the pitfalls.

## When to use
- Local and CI CodeQL finding counts disagree ("hermeticity leak?").
- You need to reproduce the CI number locally and trust it.
- You want to test query changes from an out-of-tree `codeql-coding-standards`
  checkout (e.g. verifying upstream false-positive fixes before they are released/pinned).
- You are bumping the CodeQL bundle or the coding-standards pack version.

## Invoke
```
/skill:codeql-analysis
```

## What it covers
- How the two-phase pipeline works (`create-database` traced build → `analyze-database`).
- The `/var/tmp` sandbox-writable requirement (else silent empty databases).
- How to build one database and reuse it across query packs.
- Recipe A: reproduce CI locally.
- Recipe B: analyze a mainline/dev `codeql-coding-standards` checkout — including the two
  non-obvious traps (cached pack shadowing, and `--rerun` for cached query results).
- Recipe C: analyze the QNX build (`--build-config qnx`) and deduplicate against Linux, on the
  SARIF documents, into a single union that is consumed directly by the quality dashboard
  (no CSV export).
- Diagnosing local-vs-CI divergence via the GitHub Pages published SARIF + nightly `headSha`
  provenance, and the rule-fingerprint method for classifying a difference.

## Key facts (see SKILL.md for detail)
- Databases MUST live under `/var/tmp` (`--sandbox_writable_path=/var/tmp`).
- Verify DB completeness: release pack ≈ 2717 total, RULE-8-2-10 ≈ 106 (not ~1).
- CI's published number can be stale — always check the nightly's `headSha`.
- Compiler version and `--jobs` do NOT change counts; bundle/pack version does.
