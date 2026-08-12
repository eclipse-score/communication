---
name: codeql-analysis
description: Run, reproduce, and debug the repository's CodeQL/MISRA static analysis. Explains the hermetic pipeline, how to reproduce CI locally, how to diagnose finding-count divergence, and how to analyze against an out-of-tree (mainline/dev) codeql-coding-standards checkout without re-extracting.
---

# CodeQL / MISRA Analysis Skill

## Purpose
Everything needed to work on `//quality/static_analysis:codeql_lint` without re-discovering
the pitfalls. Use it when:
- local and CI finding counts disagree ("hermeticity leak?"),
- you need to reproduce CI numbers locally,
- you want to test query changes from an out-of-tree `codeql-coding-standards`
  checkout (e.g. verifying upstream false-positive fixes before they are released),
- you are bumping the CodeQL bundle or the coding-standards pack version.

## The pipeline in one picture
`bazel run //quality/static_analysis:codeql_lint -- --target //score/message_passing //score/mw/com`

`codeql_lint.py` does two phases (see `--phase create-database|analyze-database|all`):

1. **create-database** — `codeql database init --begin-tracing`, then a **traced**
   `bazel build --config=codeql --stamp --action_env=CODEQL_SEED_FORCE_RECOMPILE=<timestamp>`,
   then `codeql database finalize`. The seed is a fresh timestamp each run and lands in every
   `CppCompile` action's environment, so `--nouse_action_cache` + the seed force a full
   recompile-and-trace every time (no TU is skipped).
2. **analyze-database** — runs the **vendored pre-compiled** MISRA pack
   (`@codeql_coding_standards_compiled//:pack`, referenced as `<name>@<version>:codeql-suites/misra-cpp-default.qls`
   via `--search-path`). `--query-spec` overrides this to run a single query from the
   vendored **source** pack (`@codeql_coding_standards`).

CI (`.github/workflows/_codeql.yml`, invoked by `nightly_quality.yml`, cron `0 0 * * *`) runs
this command for the **Linux** config, and — automatically, whenever the `SCORE_QNX_LICENSE`
secret is provided — a second time for the **QNX** config (`--build-config qnx`), then
deduplicates on SARIF into a single union (Recipe C). For a single-config run the command is
**identical** to the local one, so any local-vs-CI difference in the Linux number is NOT in
the command.

## Key pins (MODULE.bazel)
- `codeql_bundle` — the CodeQL CLI + extractors. **URL-pinned**, e.g. `codeql-bundle-v2.21.4`.
  Bundle version strongly affects control-/data-flow rules (dead-code / unreachable-statement
  detection). It is the single biggest lever on finding counts.
- `CODING_STANDARDS_VERSION` (e.g. `2.61.0`) — drives BOTH `@codeql_coding_standards` (source +
  report scripts) and `@codeql_coding_standards_compiled` (pre-compiled query pack). Keep them
  in lock-step; the bundle and coding-standards versions must be compatible
  (see the checkout's `supported_codeql_configs.json`).

## Hard-won facts (do not relearn these)

### 1. The database MUST live under `/var/tmp`
`quality/static_analysis/static_analysis.bazelrc` sets `--sandbox_writable_path=/var/tmp`, and
`codeql_lint.py` uses `TMP_PATH_FOR_DATABASES = /var/tmp/codeql_databases`. During the traced
build, clang runs **inside the bazel linux-sandbox**; the CodeQL tracer must write TRAP files to
`CODEQL_EXTRACTOR_CPP_TRAP_DIR` (inside the DB dir). The sandbox makes everything **outside**
`/var/tmp` read-only, so a DB anywhere else (e.g. `/tmp/...`) yields **silent empty extraction**:
`src.zip` has ~2 files, relations ≈ 900 B, and analysis returns **0 findings** with no error.
→ Always pass `--database-path /var/tmp/codeql_databases/<name>`.

### 2. Verify a database is complete before trusting counts
`create-database` can occasionally produce an under-populated call-graph DB (whole-program rules
like RULE-8-2-10 collapse) even though per-file coverage looks normal. Sanity gate on a
**complete** DB (score/message_passing + score/mw/com):
- `src.zip` ≈ 3.7 MiB, relations ≈ 37 MiB.
- Release pack total ≈ **2717**, and **RULE-8-2-10 ≈ 106** (NOT ~1).
If RULE-8-2-10 is ~1, the DB is incomplete — rebuild it (a fresh `--phase create-database` fixed it).

### 3. Reuse the database; it is independent of the query pack
Extraction (the 5–6 min traced build) does not depend on which queries you run. Build the DB
**once**, then analyze it many times with different packs. `--phase all` builds the DB in a
`TemporaryDirectory` and deletes it — use `--phase create-database --database-path ...` to keep it.

### 4. Diagnosing local-vs-CI divergence: check the published number's provenance FIRST
CI publishes its results to GitHub Pages (no auth needed):
`https://eclipse-score.github.io/communication/latest/quality/codeql_findings.txt` (full CSV) and
`.../latest/quality/index.html` (dashboard KPI + "Generated: <UTC>").
The published number comes from the **last successful nightly**, which built whatever commit was
HEAD at cron time — often **stale** relative to your working tree. Confirm with:
```bash
gh run list --workflow=nightly_quality.yml --limit 5           # unauth OK on public repo
gh run view <run_id> --json headSha,createdAt,event
git merge-base --is-ancestor <pipeline-fix-commit> <run_headSha> && echo present || echo STALE
```
A pipeline/bundle change merged **after** the last nightly means the dashboard reflects the OLD
pipeline. This is the usual explanation for "CI has 5000+, local has 2700" — not a hermeticity leak.

### 5. Rule fingerprint tells you what kind of change happened
When comparing two result sets, split by rule ID (`grep -oE '(RULE|DIR)-[0-9]+-[0-9]+-[0-9]+'`):
- **Syntactic/AST rules identical, control-flow rules diverge** (RULE-0-0-1 unreachable
  statements, RULE-0-1-1, RULE-0-0-2, RULE-15-0-1) ⇒ **bundle/query-pack version change**, not a
  compiler/build/host difference.
- Compiler version (clang 19 vs 22) and `--jobs` do NOT change counts. Confirmed hermetic.

## Recipe A — Reproduce CI locally / get a trustworthy count
```bash
cd <repo>
bazel run //quality/static_analysis:codeql_lint -- \
  --output-dir /tmp/codeql-results --output-prefix codeql-nightly \
  --target //score/message_passing //score/mw/com
# codeql_lint only produces SARIF now (no CSV). Derive the CSV (and dedupe,
# even for a single input) via the pinned Sarif.Multitool + sarif-tools CLIs
# directly (no custom wrapper script):
bazel run @sarif_multitool//:sarif_multitool_cli -- \
  merge /tmp/codeql-results/codeql-nightly.sarif --merge-empty-logs \
  --output-file /tmp/codeql-results/codeql-nightly-final.sarif
bazel run //quality/static_analysis:sarif_cli -- \
  csv /tmp/codeql-results/codeql-nightly-final.sarif \
  --output /tmp/codeql-results/codeql-nightly-final.csv
wc -l /tmp/codeql-results/codeql-nightly-final.csv
```
~8–12 min. Deterministic (~2717 on a complete DB at time of writing). Compare against the
**provenance-checked** CI CSV from GitHub Pages, not a remembered figure.

## Recipe B — Test an out-of-tree codeql-coding-standards checkout (mainline / dev)
Use this to verify upstream query fixes (e.g. false-positive reductions) before they are pinned.
Assume the checkout is at `$CS` (e.g. `/home/jan/00_workspace/codeql-coding-standards`).

**B0. Compatibility:** check `$CS/supported_codeql_configs.json` `codeql_cli` matches the pinned
bundle version. If not, bump the bundle or check out a compatible branch.

**B1. Build ONE complete DB** (Recipe from §1/§2):
```bash
bazel run //quality/static_analysis:codeql_lint -- \
  --phase create-database --database-path /var/tmp/codeql_databases/db_fresh \
  --target //score/message_passing //score/mw/com
```

**B2. CRITICAL — stop the cached release pack from shadowing your checkout.**
`~/.codeql/packages/codeql/misra-cpp-coding-standards/<ver>` (populated from a prior release-pack
run) has the SAME pack name as the checkout. `--additional-packs=$CS` does **not** override it, and
CodeQL prefers the stable cached version over the checkout's `x.y.z-dev` prerelease — so you
silently re-run the OLD queries and see NO difference. Hide it:
```bash
mv ~/.codeql/packages/codeql/misra-cpp-coding-standards/<ver>{,.hidden}
```
(Leave `advanced-security/qtil` in the cache — the checkout needs it. `codeql/cpp-all` comes from
the bundle, `common-cpp-coding-standards` from the checkout.)

**B3. Analyze against the checkout — always with `--rerun`** (bypasses cached query *results*,
which are keyed independently of which pack you pass):
```bash
# The real CLI binary is <output_base>/external/*codeql_bundle*/codeql/codeql :
CQ=$(find "$(bazel info output_base)/external" -maxdepth 3 -path '*codeql_bundle*/codeql/codeql' -type f | head -1)
"$CQ" database analyze -j=0 --rerun /var/tmp/codeql_databases/db_fresh \
  "$CS/cpp/misra/src/codeql-suites/misra-cpp-default.qls" \
  --additional-packs="$CS" \
  --format=csv --output=/tmp/mainline.csv
```
First run compiles the queries from source (~40 min, cached afterward in `~/.codeql/compile-cache`);
reruns are ~2 min.

**B4. Baseline on the SAME DB** and diff by rule:
```bash
bazel run //quality/static_analysis:codeql_lint -- \
  --phase analyze-database --database-path /var/tmp/codeql_databases/db_fresh \
  --output-dir /tmp/relpack --output-prefix rel \
  --target //score/message_passing //score/mw/com
# codeql_lint only emits SARIF; derive rel.csv via the pinned Sarif.Multitool +
# sarif-tools CLIs directly (no custom wrapper script).
bazel run @sarif_multitool//:sarif_multitool_cli -- \
  merge /tmp/relpack/rel.sarif --merge-empty-logs \
  --output-file /tmp/relpack/rel-merged.sarif
bazel run //quality/static_analysis:sarif_cli -- \
  csv /tmp/relpack/rel-merged.sarif --output /tmp/relpack/rel.csv
for f in /tmp/relpack/rel.csv /tmp/mainline.csv; do
  grep -oE '(RULE|DIR)-[0-9]+-[0-9]+-[0-9]+' "$f" | sort | uniq -c > "$f.rules"; done
join -1 2 -2 2 -a1 -a2 -e0 -o '0,1.1,2.1' \
  <(sort -k2 /tmp/relpack/rel.csv.rules) <(sort -k2 /tmp/mainline.csv.rules) \
  | awk '$2!=$3{printf "%-14s rel=%-5s mainline=%-5s (%+d)\n",$1,$2,$3,$3-$2}'
```

**B5. Restore the cache** when finished:
```bash
mv ~/.codeql/packages/codeql/misra-cpp-coding-standards/<ver>{.hidden,}
```

## Recipe C — Analyze the QNX build and deduplicate against Linux
The nightly runs the analysis for two Bazel configs: the default **Linux** build and
`--config=qnx` (QCC toolchain, QNX SDP headers, QNX platform). They compile the **same** TUs, so
most MISRA findings are identical; only platform-gated code (`#ifdef __QNX__`, QNX headers,
QCC-deduced types) differs. We report a single **deduplicated union** (no separate QNX-only delta
is published). In CI (`_codeql.yml`) Linux and QNX each run as their own job on their own runner in
parallel (both gated only on a tiny `determine-qnx` job that resolves secret availability); a final
`merge` job waits on both and does the SARIF merge + CSV steps below.

**C0. Prerequisites** (same as any QNX build): a qnx.com account, an assigned QNX 8 license under
`/opt/score_qnx/license/licenses`, and `~/.netrc` credentials (see the QNX section in `//.bazelrc`).
`codeql_lint.py` runs `bazel build` with the ambient environment, so the QNX credential helper
picks up `SCORE_QNX_USER` / `SCORE_QNX_PASSWORD` if exported (that is how CI passes them).

**C1. Build + analyze each config into a distinct output dir** (distinct `/var/tmp` DBs; §1 still
applies to both). Each run only produces SARIF (no CSV):
```bash
# Linux (unchanged)
bazel run //quality/static_analysis:codeql_lint -- \
  --output-dir /tmp/codeql-results/linux --output-prefix codeql-nightly \
  --target //score/message_passing //score/mw/com
# QNX: layer --config=qnx onto the traced build via --build-config (repeatable)
bazel run //quality/static_analysis:codeql_lint -- \
  --build-config qnx \
  --output-dir /tmp/codeql-results/qnx --output-prefix codeql-nightly-qnx \
  --target //score/message_passing //score/mw/com
```
`--build-config qnx` ⇒ `bazel build --config=codeql --config=qnx`. The configs layer additively:
QCC matches the QNX platform, and the base `codeql` config's llvm-linux toolchain simply is not
selected for QNX-platform targets. If a real flag conflict ever appears, split the shared hermetic
flags in `static_analysis.bazelrc` into a `codeql_common` base and add a dedicated `codeql_qnx`.

**Verify the QNX DB is complete before trusting counts** (§2 applies to QCC extraction too — a
traced QCC build that silently under-populates the DB collapses whole-program rules just like the
Linux case).

**C2. Merge / deduplicate on SARIF, then derive the CSV — both tools invoked directly**
(no custom wrapper script):
```bash
bazel run @sarif_multitool//:sarif_multitool_cli -- \
  merge /tmp/codeql-results/linux/codeql-nightly.sarif \
        /tmp/codeql-results/qnx/codeql-nightly-qnx.sarif \
  --merge-empty-logs \
  --output-file /tmp/codeql-results/codeql-nightly.sarif
bazel run //quality/static_analysis:sarif_cli -- \
  csv /tmp/codeql-results/codeql-nightly.sarif \
  --output /tmp/codeql-results/codeql-nightly.csv
```
All deduplication happens on the two SARIF documents (never on a CSV) via `Sarif.Multitool merge` (the
same tool used by `merge_sarif_reports`/action.yml for rules_lint — no hand-rolled dedup key); the
binary is a pinned, sha256-verified Bazel dependency (`@sarif_multitool` in `MODULE.bazel`, fetched by
URL like `@codeql_bundle` — not via `npx`/npm at runtime, so no network access is needed). The CSV is
generated last, directly from the resulting union SARIF, via the `sarif` CLI from `sarif-tools`
(`//quality/static_analysis:sarif_cli`, a `py_console_script_binary` — again no custom wrapper script).
Writes a single `codeql-nightly.{sarif,csv}` — the deduplicated union (linux ∪ qnx); every distinct
finding across both configs exactly once. Invariant to sanity-check: `union <= linux + qnx` (equality
only if there is no overlap). CI uploads the union under Code Scanning category `codeql-nightly`; the
union CSV feeds the dashboard KPI (there is no QNX-only delta category, artifact, or dashboard column
anymore).

## Worked example (2026-07, for calibration)
- Bundle `2.21.4`, pinned pack `2.61.0`, checkout branch on `2.62.0-dev`.
- Same complete DB: release pack **2717**, mainline checkout **2559** (−158).
- Two rules changed, both upstream FP fixes:
  - RULE-8-2-10 (recursion; callers of recursive fns) 106 → 1 — PR #1141 / 882c472.
  - RULE-6-9-2 (std integer type names; `auto`-deduced types) 55 → 3 — PR #1147 / 4ca4133a.
- Separately, a "CI 5716 vs local 2717" scare was a **stale nightly**: the published dashboard
  came from a run whose `headSha` predated the bundle-downgrade + vendored-pack rewrite (bundle
  had been `2.26.0`, which inflated RULE-0-0-1 unreachable-statement findings to ~2593).

## Gotcha checklist (fast triage)
- 0 findings / empty `src.zip` → DB not under `/var/tmp` (§1).
- Checkout changes have "no effect" → cached pack shadowing (§B2) and/or missing `--rerun` (§B3).
- RULE-8-2-10 ≈ 1 with the release pack → incomplete DB, rebuild (§2).
- "CI has way more findings" → check the published number's `headSha` provenance before assuming a
  leak (§4); most likely a stale nightly or a bundle/pack version delta (§5).
- Compiler version / `--jobs` are NOT the cause — do not chase them.
