# S-CORE Quality Maintenance Proposal

**Author:** Mahale Komal
**Date:** May 6, 2026
**For:** Ahmed & Jan

---

## Summary

This document describes how S-CORE can define, track, and enforce software quality goals (KPIs).

**Quality goals we want to reach:**
- 100% line coverage
- No clang-tidy issues
- No CodeQL security issues
- No compiler warnings

The main question is not just which goals to set, but how we maintain them as a team.

**Recommended approach: Hybrid Quality Model**

| Check Type | When It Runs | Examples |
|------------|-------------|---------|
| Fast checks | Every PR | Build, unit tests, formatting, basic lint |
| Heavy checks | Once after review (manual) or every night | CodeQL, clang-tidy, sanitizers, coverage |
| Reporting | After every nightly run | Shared quality dashboard |

---

## Problem

Today we can set quality goals, but we have no clear way to:
- See the current status of each KPI
- Stop quality going down over time
- Block bad code from being merged

This proposal compares three options and recommends one for the team to decide.

---

## Options

### Option 1: Dashboard Only

Track quality numbers in a shared dashboard. No automatic blocking.

| Pros | Cons |
|------|------|
| Clear visibility of quality status | Does not block bad code from being merged |
| Easy to show trends over time | Needs manual team reaction |
| Good for management reporting | Not useful if nobody checks the dashboard |

---

### Option 2: CI Enforcement Only

Block merges in CI if quality thresholds are not met.

| Pros | Cons |
|------|------|
| Automatically enforces standards | Can slow development if thresholds are too strict |
| Prevents regressions | May cause friction if codebase is not ready |
| Makes expectations clear | Requires careful setup and baseline agreement |

---

### Option 3: Combined Approach — Recommended

Use both: dashboard for visibility and CI for enforcement of the most important checks.

Heavy checks (CodeQL, clang-tidy, coverage) run in one of two ways:
- **Manual trigger** — developer triggers once after review is done, before merge
- **Nightly schedule** — runs automatically every night for continuous monitoring

This means expensive jobs do not run on every commit, but still run before merge.

| Pros | Cons |
|------|------|
| Balances speed and quality | Needs more setup |
| Prevents regressions and shows trends | Requires agreement on what to enforce vs. track |
| PR feedback stays fast | Developer must remember to trigger before merge |
| Avoids wasted CI runs on in-progress commits | |

---

## Measured CI Runtimes

These times were measured from a real CI run on this repository (PR #398):

| Check | Measured Time |
|-------|--------------|
| CodeQL analysis | ~5 hours 56 minutes |
| Clang-Tidy analysis | ~44 minutes |
| Coverage analysis | ~33 minutes |

**Key observations:**
- CodeQL is very slow (~6 hours) because it traces all compilations, builds a security database, and runs queries. Running it on every commit is not practical.
- Clang-tidy and coverage analysis are moderately fast but still too slow for every PR commit.
- The manual post-review trigger is the best fit — runs once when it matters, does not slow down normal development.
- These are from one run. We need more runs to confirm average times.
- Future work: CodeQL runtime can be reduced by scoping targets and tuning cache settings.

---

## Findings Fix Policy

When a check finds an issue, this policy applies:

| Severity | Fix Deadline |
|----------|-------------|
| Critical (e.g. CodeQL security finding) | Must be fixed before merge |
| High | Owner assigned, fixed within 1–2 working days |
| Medium / Low | Added to backlog with owner and due date |

**Rules:**
- Every finding must have an assigned owner.
- Nightly failures are reviewed at the start of the next working day.
- If a critical finding is found after a merge, a fix PR must be raised immediately.

---

## Frequently Asked Questions

**Can we reduce CodeQL runtime?**
Yes. We can limit which targets CodeQL scans, improve caching, and run it only at controlled points (manual post-review or nightly).

**Should heavy checks block merge?**
Fast checks block merge on every PR. Heavy checks run manually after review or nightly. Critical findings from heavy checks must be fixed before the next merge.

**How does this affect developers?**
PR feedback stays fast (5–10 minutes). Heavy checks only run once after review is complete, not on every commit.

**How do we handle findings?**
Critical findings block merge. High severity fixed in 1–2 days. Medium and low tracked in backlog with owner and due date.

**Is coverage timing final?**
The 33-minute time is from the current setup. If test scope or coverage tools change, runtime may change. We will re-measure after configuration changes.

**How do we know the approach is working?**
We track per-job duration, pass/fail results, and findings count over time. We review whether quality improves and whether developer wait time is acceptable.

**What do you recommend right now?**
Start with the hybrid model. Run heavy checks manually after review and nightly. Focus first on reducing CodeQL runtime since it is the biggest bottleneck at ~6 hours.

---

## What Is Already in S-CORE

| Item | Status |
|------|--------|
| PR build and test checks | Present |
| Formatting checks | Present |
| Coverage and sanitizer workflows | Present but not PR-blocking |
| Dedicated CodeQL workflow | Missing |
| Nightly scheduled quality workflow | Missing |
| KPI export to shared dashboard | Missing |

---

## Implementation Plan

### Phase 1 — Start Heavy Quality Jobs
- [ ] Create nightly workflow for CodeQL and full clang-tidy
- [ ] Create nightly workflow for sanitizers (TSAN, ASAN, LSAN)
- [ ] Add manual trigger option for post-review runs
- [ ] Export coverage result from nightly job
- [ ] Define fix-time rules for failures

### Phase 2 — Add Shared Dashboard
- [ ] Agree on which numbers to show (coverage, warnings, test pass rate, defects)
- [ ] Choose dashboard tool (see options below)
- [ ] Push nightly results to dashboard
- [ ] Share dashboard with S-CORE teams

---

## Dashboard Tool Options

| Option | Cost | Best For |
|--------|------|---------|
| **Grafana** | Free (open source) | Trend charts, team dashboards |
| **Static HTML Dashboard** | Free | Simple KPI reports, low setup effort |
| **SonarQube Community** | Free (self-hosted) | Code quality and coverage reporting |
| **Power BI** | Free desktop, paid cloud | Good visualization if company license exists |

---

## Recommendation

Use **Option 3: Combined Approach (Hybrid Model)**.

**Why:**
- Fast PR checks keep developer feedback under 10 minutes.
- Heavy checks run manually after review or nightly — not on every commit.
- CI enforces the most critical rules (no compiler warnings, no critical CodeQL findings).
- Dashboard tracks broader trends (coverage progress, quality over time).
- This approach is practical and can be rolled out step by step.

**Next Steps:**

1. Agree on the final list of KPIs to track and enforce.
2. Decide which KPIs block merge in CI and which are tracked in the dashboard only.
3. Run a short pilot: nightly CodeQL + coverage.
4. Review results with the team.
5. Choose a dashboard tool and start rollout.
