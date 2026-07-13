# Bazel Cache Strategy — Design Document

## 1. Overview

This document describes the caching strategy for Bazel builds in the CI/CD
pipelines of the `eclipse-score-communication` repository. It defines
requirements, design decisions, and the implementation architecture.

## 2. Goals

- **Faster PR feedback** — pull requests and merge-queue runs restore
  pre-populated caches so builds complete significantly faster.
- **Cache freshness** — caches are recreated nightly to prevent staleness and
  unbounded growth.
- **Correctness** — cache usage never masks build failures; caches are
  read-only for untrusted code (pull requests).
- **Simplicity** — a single, well-documented mechanism replaces the
  previously opaque third-party action.

## 3. Requirements

### 3.1 Cache Types

| Cache | Bazel Flag | Content |
|-------|-----------|---------|
| **Repository cache** | `--repository_cache` | Downloaded external repositories (tarballs, git repos). Shared across all jobs. |
| **Disk cache** | `--disk_cache` | Build action outputs (object files, test results). One per job type. |

### 3.2 Behavioral Requirements

| ID | Requirement |
|----|-------------|
| R1 | Jobs triggered by **pull request** or **merge queue** SHALL use both caches in **read-only** mode (restore only, no save). |
| R2 | Jobs triggered by **push to main** SHALL **update** the disk cache (restore existing → execute → save new → delete old entry). |
| R3 | Jobs triggered by **push to main** SHALL use the repository cache in **read-only** mode. |
| R4 | Jobs triggered by triggers **other than** pull request, merge queue, or push to main SHALL **not** use any caches (unless explicitly opted in via the `cache-mode` / `cache_mode` input). |
| R5 | The **repository cache** SHALL be **recreated nightly** — no old cache is restored, the job runs from scratch, and a fresh cache is saved. |
| R6 | The **disk cache** SHALL be **recreated nightly** — same semantics as R5, applied per job type. |
| R7 | After recreation, **old cache entries** SHALL be **deleted** from the GitHub Actions cache store. |
| R8 | Nightly recreation SHALL also be **triggerable manually** via `workflow_dispatch`. |
| R9 | During nightly recreation, jobs SHALL run **sequentially** to avoid cache key conflicts and excessive parallel resource consumption. |
| R10 | The repository cache SHALL be **shared** across all jobs (single cache key based on content hash of Bazel's `content_addressable/sha256/` directory). |
| R11 | Disk caches SHALL be **per-job** (unique key per workflow/configuration). |

### 3.3 Non-Requirements

- Remote build caches (BuildBuddy, etc.) are out of scope.
- Bazelisk binary caching is handled by `bazel-contrib/setup-bazel` (`bazelisk-cache`). It is only
  enabled in write modes (`update-disk`, `recreate`, `recreate-update`) to avoid spurious
  "cache write denied" warnings in read-only and disabled modes.

## 4. Architecture

### 4.1 Workflow Organization

Workflows are organized into two layers:

**Layer 1 — Reusable workflows** (job logic defined once, `workflow_call`):

| Workflow | Description |
|----------|-------------|
| `_build_and_test_gcc15.yml` | Full build + test + examples (GCC 15) |
| `_thread_sanitizer.yml` | Tests with `--config=tsan` |
| `_address_sanitizer.yml` | Tests with `--config=asan_ubsan_lsan` |
| `_clang_tidy.yml` | Static analysis with findings collection and artifact upload |
| `_codeql.yml` | CodeQL / MISRA analysis with SARIF + CSV upload |
| `_coverage_report.yml` | Coverage report with HTML archive and artifact upload |
| `build_and_test_qnx.yml` | QNX cross-compilation with approval gate and secrets |

Each reusable workflow accepts two inputs:
- `cache-mode` (string) — override, empty = auto-compute from event context
- `fetch-only` (string) — when `"true"`, runs `--nobuild` to fetch only

Disk cache names are **internal** to each workflow and not exposed.

**Layer 2 — Orchestrators** (arrange execution pattern):

| Orchestrator | Calls | Execution |
|-------------|-------|-----------|
| `build_and_test_host.yml` | host, tsan, asan, clang-tidy (reusable workflows) | **Parallel** (PR/merge_group) |
| `nightly_cache_recreation.yml` | All 7 job types (reusable workflows) | **Sequential** fetch → **Parallel** build |
| `nightly_quality.yml` | coverage, clang-tidy, codeql (reusable workflows) | **Parallel** (KPI reporting) |
| `automated_release.yml` | `build_and_test_host`, QNX, coverage | **Parallel** (via reusable workflows) |

Nesting depth: `automated_release → build_and_test_host → _build_and_test_gcc15.yml` = 2 levels (max 4 allowed).

### 4.2 Components

```
.github/
├── actions/
│   └── 00_infrastructure/
│       ├── prepare_bazel_environment/                 # Composite action: env setup + calls post hook
│       │   └── action.yml
│       └── setup_bazel_cache/     # Node.js action: main (restore) + post (save)
│           ├── action.yml
│           ├── src/main.js
│           ├── src/post.js
│           ├── package.json
│           └── dist/                  # Bundled with @vercel/ncc
├── workflows/
│   │  # Layer 1 — Reusable workflows (workflow_call)
│   ├── _build_and_test_gcc15.yml
│   ├── _thread_sanitizer.yml
│   ├── _address_sanitizer.yml
│   ├── _clang_tidy.yml
│   ├── _codeql.yml
│   ├── _coverage_report.yml
│   ├── build_and_test_qnx.yml
│   │  # Layer 2 — Orchestrators
│   ├── build_and_test_host.yml        # PR quality gates (parallel)
│   ├── nightly_cache_recreation.yml   # Nightly sequential cache rebuild
│   ├── nightly_quality.yml            # Nightly KPI reporting
│   ├── automated_release.yml          # Release process
│   │  # Standalone
│   ├── deploy_docs.yml
│   └── stale_pr.yml
└── cache-strategy.md
```

### 4.3 Cache Modes

Cache mode is controlled by a single `cache-mode` input (composite actions)
or `cache_mode` input (callable workflows). When empty (the default), the
mode is auto-computed from the GitHub event context:

```yaml
# In prepare_bazel_environment (composite action) — computed automatically:
CACHE_MODE: >-
  ${{
    inputs.cache-mode != '' && inputs.cache-mode ||
    (github.event_name == 'pull_request' || github.event_name == 'merge_group') && 'read-only' ||
    (github.event_name == 'push' && github.ref == 'refs/heads/main') && 'update-disk' ||
    'disabled'
  }}
```

Orchestrators pass an explicit value (e.g., `cache-mode: "recreate"`) to
override the auto-computed mode during nightly recreation.

| Mode | Restore Disk | Restore Repo | Save Disk | Save Repo | Delete Old |
|------|:---:|:---:|:---:|:---:|:---:|
| `read-only` | ✅ | ✅ | ❌ | ❌ | ❌ |
| `update-disk` | ✅ | ✅ | ✅ | ❌ | old disk entries |
| `recreate` | ❌ | ❌ | ✅ | ✅ (content-hash key) | old disk + repo entries |
| `recreate-update` | ❌ | ✅ (from prev job) | ✅ | ✅ (content-hash key) | old disk + repo entries |
| `disabled` | ❌ | ❌ | ❌ | ❌ | ❌ |

### 4.4 Cache Keys

| Cache | Key Pattern | Restore Keys |
|-------|-------------|-------------|
| Repository (final) | `repo-cache-<content-hash>` | `repo-cache-` (prefix match) |
| Disk | `disk-cache-<job-name>-<run_id>` | `disk-cache-<job-name>-` |

**Repository cache key rationale**: The repository cache is saved under
a content-hash key. This hash is derived from Bazel's own repository cache
structure: the `content_addressable/sha256/` directory contains files whose
**names are already their SHA-256 content hashes**. By sorting and hashing
this list of filenames, we get an accurate fingerprint of the entire cache
in milliseconds — no need to re-read multi-GB file content.

This avoids false invalidation — if nothing changed in the external
repositories overnight, the hash stays the same and the save becomes a no-op
(key already exists).

During nightly recreation, each sequential job computes its own content hash
and saves. Since jobs add new external dependencies, the hash changes with
each job. Each job deletes all old repo cache entries except its own, so only
the latest (most complete) entry survives for the next job to restore.

**Disk cache key**: Uses `run_id` to ensure each save creates a unique entry.
The `restore-keys` prefix match restores the most recent available entry.

### 4.5 Infrastructure Actions

#### `prepare_bazel_environment` (Composite Action)

The entry point used by all reusable workflows. Steps:

1. Sets environment variables (`ANDROID_HOME`, `FORCE_JAVASCRIPT_ACTIONS_TO_NODE24`).
2. Auto-computes `CACHE_MODE` from `cache-mode` input or event context.
3. Calls `setup_bazel_cache` (Node.js action with `main` + `post`).
4. Frees disk space via `eclipse-score/more-disk-space@v1`.
5. Enables linux-sandbox.

**Inputs**: `disk-cache` (name, empty to skip disk caching), `cache-mode`
(override, empty = auto-compute).

#### `setup_bazel_cache` (Node.js Action)

A Node.js action (`using: node24`) with main/post lifecycle:

- **`main`**: Saves inputs to action state, creates cache directories,
  restores caches via `@actions/cache` npm library, writes `~/.bazelrc`
  with `--repository_cache` and `--disk_cache` flags.
- **`post`** (`post-if: always()`): Saves disk cache (if name non-empty),
  saves repository cache (if mode is `recreate`/`recreate-update`),
  deletes old entries via GitHub API. Guaranteed to run even on
  job cancellation.

Both restore and save use the `@actions/cache` npm library (not the
`actions/cache` marketplace action) for consistency.

### 4.6 Nightly Recreation Flow

Two-phase approach for maximum efficiency:

**Phase 1 — Fetch (sequential)**: Each job runs `bazel build --nobuild` with
its config to trigger loading and analysis phases, fetching all config-specific
external dependencies. The repository cache accumulates across sequential jobs.
Retry up to 10 times on failure.

**Phase 2 — Build (parallel)**: All jobs run the actual builds in parallel,
restoring the complete repository cache from Phase 1. Each saves its own disk
cache.

```
┌─────────────────────────────────────────────────┐
│         nightly_cache_recreation.yml            │
│  (schedule: 0 3 * * * | workflow_dispatch)      │
└─────────────────────────────────────────────────┘
          │
          │ PHASE 1: Fetch (sequential, --nobuild)
          ▼ mode: recreate (first job, starts empty)
  ┌───────────────┐
  │ fetch-host    │──▶ bazel build --nobuild //...
  └───────────────┘    saves repo-cache-<hash1>
          │
          ▼ mode: recreate-update (restores repo from prev)
  ┌───────────────┐
  │ fetch-tsan    │──▶ bazel build --nobuild --config=tsan //...
  └───────────────┘    saves repo-cache-<hash2>
          │
          ▼ ... (asan, clang-tidy, coverage, codeql, qnx)
          │
          │ PHASE 2: Build (parallel)
          ▼ mode: recreate-update (restores complete repo cache)
  ┌────────────┬──────┬──────┬─────────────┬──────────┬────────┬─────┐
  │ host build │ tsan │ asan │ clang-tidy  │ coverage │ codeql │ qnx │
  └────────────┴──────┴──────┴─────────────┴──────────┴────────┴─────┘
       Each: full build → saves disk-cache-<name>-<run_id>
```

Note: CodeQL runs without disk cache (`disk-cache: ""`) since cached
analysis results are undesirable.

### 4.7 Push-to-Main Flow

```
PR merged → push to main
    │
    ▼
┌────────────────────────────────────┐
│ build_and_test_host.yml (parallel)     │
│  mode: update-disk                 │
│  ┌──────────────┬───────┬───────┐  │
│  │ host build   │ tsan  │ asan  │  │
│  └──────────────┴───────┴───────┘  │
│  Each: restore → run → save disk   │
└────────────────────────────────────┘
    (same for QNX, triggered separately on push)
```

## 5. Permissions

The cache save + cleanup actions require `actions: write` permission to:
- Save new cache entries
- Delete old cache entries via `DELETE /repos/{owner}/{repo}/actions/caches/{id}`

All workflows that save or delete caches already declare this permission.

## 6. Interaction with Other Workflows

| Workflow | Behavior |
|----------|----------|
| `automated_release.yml` | Calls `build_and_test_host.yml` (reusable workflows). Cache mode auto-computes to `disabled` (no PR/push trigger). |
| `nightly_quality.yml` | Uses `_coverage_report.yml`, `_clang_tidy.yml`, and `_codeql.yml` reusable workflows. Cache mode auto-computes to `disabled`. |
| `deploy_docs.yml` | Uses `prepare_bazel_environment` directly with `disk-cache: build_docs`. |
| `stale_pr.yml` | Does not use Bazel — unaffected. |

## 7. Disk Cache Names

| Reusable Workflow | Disk Cache Name |
|-------------------|----------------|
| `_build_and_test_gcc15.yml` | `build_and_test_host` |
| `_thread_sanitizer.yml` | `build_and_test_tsan` |
| `_address_sanitizer.yml` | `build_and_test_asan_ubsan_lsan` |
| `_clang_tidy.yml` | `clang_tidy` |
| `_coverage_report.yml` | `coverage_report` |
| `_codeql.yml` | _(none — empty)_ |
| `build_and_test_qnx.yml` | `build_and_test_qnx` |
| `deploy_docs.yml` | `build_docs` |

## 8. Risks and Mitigations

| Risk | Mitigation |
|------|-----------|
| Cache corruption causes build failures | Caches are recreated nightly; any corruption is automatically resolved within 24h. Manual recreation is available via `workflow_dispatch`. |
| Cache size exceeds GitHub's 10 GB limit | Old entries are deleted after each save. Nightly recreation purges all stale entries. |
| GitHub API rate limits during cache cleanup | Cleanup uses simple pagination; the number of old entries per prefix is typically 1-2. |
| Nightly recreation takes too long (sequential) | Sequential execution is intentional to avoid cache conflicts. Total time ≈ sum of individual job times; acceptable for off-hours runs. |
