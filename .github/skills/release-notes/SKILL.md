---
name: release-notes
description: Create complete, significance-focused release notes with traceable links for improvements, bug fixes, and known issues.
---

# Release Notes Skill

## Purpose
Use this skill to produce release notes that are complete, verifiable, and focused on significant user-relevant changes.

This skill is optimized for repositories where release notes are expected to:
- include all major improvements and bug fixes in the target range,
- link each listed change to its pull request,
- include known issues based on open issues and active downstream workarounds,
- avoid inventing, hiding, or minimizing significant topics,
- detect significant changes whose impact is not obvious from the PR title alone.

## Core principles
1. **Completeness over brevity for significant topics**
   Capture all substantial behavior, API, tooling, safety, and compatibility changes.
2. **Significance filtering, not changelog dumping**
   Exclude trivial renames/churn unless they change behavior, reliability, or user workflow.
3. **Traceability for every claim**
   Every listed improvement/bug fix must include a PR link.
4. **Known issues must be actionable**
   Use open issues and active patches/workarounds; include links and impact context.
5. **PR titles are hints, not evidence**
   Review changed files, commit messages, tests, and PR descriptions to detect important behavior changes that titles understate or omit.
6. **No fabrication, no omission of major items**
   If uncertain, state uncertainty explicitly.
7. **Do not prune significant topics for brevity**
   If a topic is significant, keep it as an explicit release-note item even when the release notes become long.

When in doubt, prefer a broader semantic review over relying on PR titles alone. Several high-impact changes are easy to under-classify from titles only, especially in areas like method/field semantics, event lifecycle behavior, binding/resource management, and cross-domain/service-discovery support.

## Inputs expected
- New release tag (if non-existent referring to current HEAD).
- Origin tag (typically previous semantic version tag on the same branch).
- Release template (.github/RELEASE_TEMPLATE.md).
- Repository history and issue tracker access.

### Template handling rules (must follow)
1. **Locate template first**
   - Check whether `.github/RELEASE_TEMPLATE.md` exists.
   - If missing, stop immediately.
2. **If user asks to adapt/update markdown in the repo**
   - Write/update the target markdown file in the repository.
   - Do not only provide release notes in chat; ensure in-repo artifact exists.
3**If both template and existing release-note file exist**
   - Keep template headings intact and adapt content to that shape.
   - Preserve existing repo conventions (path, naming, section order).

## Release-note workflow

### 1) Define release scope
- Confirm release tag commit hash and date.
- Identify origin tag from the same branch and validate ancestry.
- Use the range: `origin_tag..release_tag`.

### 2) Build evidence set
Collect and review:
- Merged PRs in the range.
- Commit history in the range.
- The changed files and diff themes for each substantial PR, not just the title.
- PR descriptions, labels, and merge commit bodies when available.
- Added or modified tests, examples, docs, requirements, and configuration, because these often reveal user-visible scope that the title omits.
- Open issues relevant to stability, compatibility, and build/use regressions.
- In-repo patches/workarounds in examples or integration modules that indicate unresolved upstream issues.
- Top-level and module-level build/dependency configuration files that may encode version restrictions or temporary dependency workarounds.

Issue-tracker sweep is mandatory (do not rely only on in-repo references):
- Query open issues in the release repository.
- Run at least one broad query and one compatibility-focused query set (for example: `compatibility`, `Bazel`, `rules_python`, `Python`, `toolchain`, `build fails`).
- Record explicit include/exclude decisions for high-impact compatibility issues, especially for build-system and language-runtime compatibility.
- If issue-tracker access is unavailable, state that explicitly and mark Known Issues as potentially incomplete.

Compatibility/configuration sweep is mandatory (do not rely only on PR titles or issue tracker):
- Inspect the repository's primary build/dependency entry points (for example: `MODULE.bazel`, `WORKSPACE`, top-level `BUILD`, lock files, `requirements*.txt`, `pyproject.toml`, `package*.json`, toolchain config, and top-level `README` prerequisites).
- Look specifically for explicit or implicit version restrictions on Bazel, Python, `rules_python`, toolchains, compilers, and platform SDKs.
- Search for local dependency overrides and patches (`single_version_override`, `archive_override`, `git_override`, `patches = [...]`, files under `third_party/**`, `patches/**`, or similarly named workaround directories).
- Treat active downstream patches against external dependencies as Known Issues candidates unless the evidence clearly shows the upstream issue is fully resolved in the shipped dependency set.
- For each detected restriction or workaround, record whether it affects only local development, root-module usage, transitive consumers, or released artifacts.

Create a **candidate inventory** before writing bullets:
- Build a long-list of significant candidates from PRs, merge commits, and high-churn directories.
- Group candidates into impact clusters (for example: runtime semantics, configuration contract, test/quality infrastructure, CI/release process, docs/safety artifacts).
- Include a dedicated compatibility/workaround cluster for build-system constraints, language-runtime constraints, and local dependency patches.
- Keep an include/exclude decision note per cluster with one-line rationale.

**Cluster completeness sweep (mandatory for every identified cluster):**
After identifying a feature cluster (for example: gateway, field/method semantics, Sphinx/docs, safety artifacts), explicitly enumerate **all** PRs in the range that touch the same domain — including docs, README, test, and configuration companion PRs. Do not stop at the first headline PR. A common failure mode is: topic is covered by one PR → adjacent PRs in the same domain are silently skipped.

- For each domain cluster, list every PR that mentions that domain in its title or branch name, then decide include/exclude per PR, not per topic.
- Satellite PRs (documentation, README, tests, config updates accompanying a feature) must be captured under the same cluster bullet, not treated as implicitly covered.

**Large-scale file-churn rule:**
If a single PR touches a large number of files (rough threshold: ≥ 30 files), treat it as a standalone modernization or migration event deserving its own release-note bullet, regardless of whether the topic already has adjacent coverage. Do not collapse it into an existing bullet.

**PR number verification (mandatory before writing bullets):**
Before including any PR number in release notes, verify it:
1. Exists in the repository (not a 404).
2. Falls within the release range (`origin_tag..release_tag`), i.e., its merge date is after the origin tag date.
Do not carry PR numbers forward from prior art, user-provided lists, or assumptions without this check. Fabricated or out-of-range PR numbers invalidate traceability.

### 3) Classify changes by significance
Classify into:
- **Improvements**: new capabilities, API surface evolution, coverage/quality system upgrades, docs/requirements artifacts, tooling/pipeline changes that affect users or maintainers.
- **Bug Fixes**: behavior corrections, reliability fixes, regression fixes, race conditions, lifecycle/compatibility/build correctness fixes.
- **Known Issues**: open defects/regressions, version incompatibilities, unresolved upstream constraints, active workaround patches.

Also look for significant changes that may be hidden behind generic PR titles such as "cleanup", "refactor", "tests", "docs", "build", or "maintenance". These often contain meaningful behavior, compatibility, or workflow changes that still belong in release notes.

### 4) Decide what is significant
Treat as significant when at least one applies:
- User-visible behavior/API changed.
- Build/test/CI reliability materially changed.
- Safety/requirements/documentation artifacts materially expanded.
- Compatibility matrix or supported toolchain behavior changed.
- Build-system or language-runtime support is restricted to a specific version range, pinned interpreter, or patched dependency combination.
- Recurring or high-impact defect addressed.
- Tests/examples/docs/config changes clearly indicate a new supported workflow, a changed contract, a new limitation, or a corrected expectation.
- A "refactor" or "cleanup" PR actually changes semantics, lifecycle ordering, resource ownership, defaults, error handling, timing, or interoperability.
- A local patch or override exists because an upstream dependency, SDK, or tool version is incompatible or unresolved.

Usually de-prioritize:
- Pure formatting/renames with no functional impact.
- Narrow internal cleanup with no observable effect.

**Rename / abstract branch name warning:**
A PR titled "rename X to Y" or with an abstract internal branch name (for example `brem_add_non_movable_ref`, `js_reduce_visibility`) must **not** be dismissed as trivial without inspection. Rename PRs often establish a new semantic model or formalize a new API contract. Abstract branch names frequently hide major capability additions. Inspect the changed files before deciding to exclude.

**Test expansion classification rule:**
Broad expansions of test coverage (multiple new test files, new integration test suites, new DCOV/branch coverage infrastructure) are **Improvements**, not Bug Fixes, even when individual tests exercise a corrected behavior. Do not place a test-expansion cluster entirely inside Bug Fixes.

**Dual-classification rule:**
When a PR both fixes a defect and hardens or extends a contract (for example: adding `noexcept` to a public interface, adding an error return to a factory, replacing a raw pointer with a result type), it belongs in **both** Improvements and Bug Fixes, or in Improvements with a note that it also corrects a defect. Do not silently drop the improvement framing because the PR has fix semantics.

Before de-prioritizing a PR with a bland title, verify that it does **not**:
- alter public/protected interfaces, structs, enums, traits, function signatures, or generated artifacts,
- change test expectations around ordering, retries, timing, errors, or resource cleanup,
- add/remove compatibility notes, dependency constraints, examples, or migration guidance,
- touch integration boundaries such as FFI, IPC, networking, serialization, service discovery, or build toolchains.

## Detect hidden significant changes
Use this review heuristic for any PR whose title may understate impact:

1. **Inspect the changed files by domain**
   - Runtime/library source: check for semantic or lifecycle changes.
   - Public headers/APIs/types: check for contract evolution.
   - Tests: infer intended behavior changes and regressions fixed.
   - Examples/docs: infer changed user workflows, requirements, or limitations.
   - Build/dependency/tooling files: detect compatibility or reproducibility impact.
2. **Read the PR body or merge commit text**
   Look for rationale, follow-up notes, caveats, or linked issues that are more informative than the title.
3. **Compare title vs. actual impact**
   If the title sounds narrow but the diff spans multiple domains, split the release-note coverage into separate bullets by impact area.
4. **Use tests as evidence**
   When new or changed tests encode a behavior expectation, treat that expectation as a strong signal of release-note significance.
5. **Prefer impact wording over implementation wording**
   Write what changed for users/maintainers, not just that code was rearranged.
6. **Watch for incremental satellite PRs around a major PR**
   After capturing a major PR for a domain (for example: a Sphinx restructure, a CI architecture overhaul, a safety analysis document), actively search for additional PRs touching the same domain. Do not assume that the headline PR represents the full extent of that cluster. Incremental enhancements (CSS updates, PlantUML additions, FMEA targets, requirements files) in the same domain are commonly missed when the "topic is already covered."
7. **Topic de-duplication trap**
   Having one bullet for a topic does not mean all significant PRs in that topic are represented. For each identified topic cluster, enumerate individual PRs explicitly before compression. "Already have field config covered" is not a reason to skip inspection of additional field-related PRs that may introduce a fundamentally different capability (for example: a new tag-based API replacing boolean flags).
8. **Config-encoded known-issue trap**
   Important Known Issues often do not appear in PR titles, commit messages, or issue references at all. They may exist only as comments or overrides in files like `MODULE.bazel`, `requirements_lock.txt`, toolchain declarations, or local patch files. Always inspect these sources directly before finalizing Compatibility and Known Issues.

### 5) Write sections with meaningful grouping
Within **Improvements** and **Bug Fixes**:
- Group by domain (for example: runtime/API, Rust/FFI, quality/tooling, docs/safety).
- Use concise bullets that explain impact first, then PR links.
- Do not trust merge titles alone; inspect the changed files and merge commit body when a PR may hide multiple significant themes.
- When a PR title is generic or incomplete, derive the bullet text from the actual behavioral, compatibility, or workflow impact seen in the diff.
- Split broad PRs across multiple bullets when they affect several significant themes (for example: method semantics plus tests, or lifecycle fixes plus binding cleanup).
- Ensure each bullet has one or more linked PRs.
- Start from the long-list and compress only after coverage is complete; do not start from a short hand-picked subset.
- Never drop a significant topic only to keep the list short; instead, keep coverage complete and shorten wording per bullet if needed.
- If a broad release range has many substantial clusters, ensure section length reflects that breadth.
- If a significant cluster is intentionally omitted, record a brief exclusion rationale.

Coverage depth rule for broad releases:
- If the range has high churn (for example >80 merged PRs or >4 high-churn domains), avoid collapsing into a small handful of mega-bullets.
- Split by impact so that major clusters are independently visible (for example: gateway, field/method semantics, shared-memory lifecycle, message passing, CI architecture, quality dashboards, Sphinx/docs, safety artifacts).
- For broad releases, target enough bullets that reviewers can trace all major clusters without reading PR diffs (typically 10+ total bullets across Improvements and Bug Fixes).
- For very broad releases (for example >150 merged PRs or >8 high-impact clusters), use a higher floor (typically 14+ total bullets, with at least 6 in Improvements unless evidence shows otherwise).
- Big releases warrant big changelogs: when significance breadth is high, favor full topic coverage over short output.
- Do not collapse major modernization/removal topics into generic bullets. Keep these as separate topics when present: language/API migrations (for example `std::optional` migration), feature removals/deprecations (for example SOME/IP removal), method/field semantic model changes, and CI architecture shifts.

Within **Known Issues**:
- Group by domain (for example: runtime/API, Rust/FFI, quality/tooling, docs/safety).
- Prefer high-impact unresolved issues.
- Include issue links.
- Use concise bullets that explain impact
- Include workaround context where relevant (for example active patch in examples module).
- Include ecosystem compatibility issues explicitly when open (at minimum: Bazel version compatibility and Python/rules_python compatibility, if applicable to the repo).
- Include active local patch/workaround notes when the repo ships with patched external dependencies, especially for SDK, OS, Bazel-module, or ruleset compatibility.

### 6) Verify completeness before finalizing
Run this checklist:
- [ ] Metadata filled (release tag, origin tag, commit hash, date).
- [ ] Template resolution done (template found and used, or fallback structure used with explicit note).
- [ ] Significant improvements represented across major changed domains.
- [ ] Significant bug fixes represented across major changed domains.
- [ ] Candidate inventory built from PRs, commit history, and high-churn directories.
- [ ] Each significant cluster is either represented in release notes or explicitly excluded with rationale.
- [ ] No significant topic is removed solely for brevity; any omission has explicit rationale.
- [ ] Reviewed bland/generic PR titles for hidden significant impact.
- [ ] Reviewed all PR titles containing "rename", "cleanup", or an abstract internal branch name for hidden API/semantic impact.
- [ ] Checked tests/examples/docs/build files for user-visible meaning not stated in titles.
- [ ] Every listed improvement/fix includes PR link(s).
- [ ] All PR numbers have been verified to exist in the repository and fall within the release range (not out-of-range, not 404).
- [ ] For each identified domain cluster, all companion/satellite PRs (docs, README, tests, config) have been reviewed and either included or explicitly excluded.
- [ ] No large-scale file-churn PR (≥ 30 files) has been silently collapsed into an adjacent bullet without its own explicit coverage.
- [ ] Test-expansion clusters are represented in Improvements, not only in Bug Fixes.
- [ ] PRs with dual fix+improvement nature are represented on the improvement side as well as the fix side where warranted.
- [ ] Known issues include open issue links and active patch/workaround notes.
- [ ] Any open major compatibility issue found in the sweep is represented in Known Issues (or explicitly excluded with rationale).
- [ ] Reviewed top-level build/dependency configuration (`MODULE.bazel`/`WORKSPACE`/lock files/README prerequisites) for Bazel, Python, ruleset, compiler, and SDK version restrictions.
- [ ] Reviewed local dependency overrides and patch files; any unresolved downstream workaround is represented in Known Issues or explicitly excluded with rationale.
- [ ] Checked whether the repo requires a patched dependency combination only when used as the root module, and documented that scope if relevant.
- [ ] Improvements section length is proportional to release breadth (no under-sized Improvements with over-sized Bug Fixes in broad releases).
- [ ] No major change in range is omitted without rationale.
- [ ] No claims without evidence.

## Quality bar for final output
- Accurate and evidence-backed.
- Focused on significance, not noise.
- Traceable links for reviewers.
- Honest about unresolved risks.
- Ready for release publication with minimal editorial work.

## Anti-patterns to avoid
- Listing only a few obvious fixes while missing major themes.
- Trusting PR titles so literally that important semantic, compatibility, or workflow changes are omitted.
- Over-aggregating broad releases into overly short sections that hide major clusters.
- Mixing improvements and bug fixes without structure.
- Adding bullets without PR links.
- Ignoring missing-template handling or pretending a template was applied when none was found.
- Over-pruning for brevity so that broad/high-churn releases are summarized as only a handful of bullets.
- Dropping a significant topic just to keep the changelog short.
- Skipping an explicit include/exclude rationale for significant clusters discovered during evidence review.
- Ignoring open compatibility regressions.
- Ignoring active workaround patches in examples/integration areas.
- Relying only on repository text search and missing issue-tracker-only compatibility bugs (for example Bazel/Python/rules_python breakages).
- Relying on PR history alone and missing compatibility restrictions encoded only in build files, lock files, README prerequisites, or `MODULE.bazel` comments.
- Failing to inspect local patch files under `third_party/` or similar override locations, thereby omitting active downstream workarounds from Known Issues.
- Under-sizing the Improvements section for high-churn releases by over-aggregating distinct major topics (migration, removal, CI architecture, testing expansion, docs tooling) into too few bullets.
- Using vague statements like "various improvements" without specifics.
- **Stopping cluster enumeration early**: identifying one headline PR for a domain and assuming that covers the cluster, without enumerating and inspecting every other PR that touches the same domain (satellite/companion PRs such as docs, README, tests).
- **Dismissing "rename" PRs as trivial** without verifying whether they establish a new semantic model, API contract, or formalize a behavior change.
- **Dismissing abstract or author-prefixed branch names** (for example `brem_add_non_movable_ref`, `js_reduce_visibility`) without inspecting the diff; many such PRs introduce major capabilities.
- **Placing test-expansion clusters entirely in Bug Fixes**: broad test additions are infrastructure improvements and belong in Improvements.
- **Dropping the improvement framing of dual-nature PRs**: a PR that both fixes a defect and hardens a public contract belongs in Improvements as well as Bug Fixes.
- **Collapsing a large-scale modernization into an adjacent bullet**: a migration touching dozens of files (for example replacing a custom type with a stdlib type across 100+ files) warrants a dedicated bullet.
- **Carrying unverified PR numbers forward** from user-provided lists, prior notes, or assumptions; every PR number must be confirmed to exist and be within the release range before publication.

## Suggested section style
- Keep release template headings intact.
- In each section, use domain subheadings where change volume is high.
- Put highest-impact topics first.
- Prefer short, factual bullets with explicit links.

