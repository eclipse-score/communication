# Proposal: Nightly Quality Dashboard for Public Repository


## Option A: GitHub Actions + GitHub Pages

### Summary

Run nightly CI jobs via GitHub Actions on a cron schedule. A final job collects results and generates a static HTML dashboard, deployed to GitHub Pages.

### How It Works

1. A nightly job triggers at midnight
2. Parallel jobs run: CodeQL, Coverage, Clang-Tidy
3. `deploy_dashboard` job runs after all others
4. `generate_dashboard.py` produces a single-page HTML dashboard with job statuses
5. Coverage HTML report is included as a sub-page
6. Result is deployed to GitHub Pages

### User Access

- Dashboard URL: `https://<org>.github.io/<repo>/`
- Coverage report: `https://<org>.github.io/<repo>/coverage/index.html`
- Enable in repo Settings → Pages → Source: "GitHub Actions"

### Cost

- **$0** — GitHub Actions provides unlimited minutes for public repos; GitHub Pages is free for public repos (100 GB bandwidth/month, 1 GB storage)

### Pros

- Completely free, no external services needed
- Self-contained within the repository (workflow + script in-repo)
- Dashboard is publicly accessible with a stable URL
- Coverage report included alongside status
- Simple to maintain — just Python + standard GitHub Actions
- `workflow_dispatch` allows manual re-runs
- Concurrency control prevents overlapping runs

### Cons

- **No historical trends** — only shows the latest run (GitHub Pages is overwritten each deploy)
- GitHub Pages has a soft limit of 10 deploys/hour
- If all jobs fail, the dashboard still deploys (by design), but coverage artifact may be missing
- Single-page dashboard — limited interactivity
- No notifications on failure (need to add separately)

### Recommendations

1. **Add failure notifications** — Add a step to post to Slack/email/GitHub Issues on failure
2. **Historical data** — Consider committing a JSON log to a `gh-pages` branch to build trend charts
3. **Badge** — Add a workflow status badge to `README.md`:
   ```markdown
   ![Nightly Quality](https://github.com/<org>/<repo>/actions/workflows/nightly_quality.yml/badge.svg)
   ```

---

## Option B: GitHub Actions + GitHub Actions Workflow Badges Only (Minimal)

### Summary

Skip the custom dashboard entirely. Rely on GitHub's built-in workflow status badges and the Actions tab for visibility.

### How It Works

1. Same `nightly_quality.yml` (without the `deploy_dashboard` job)
2. Add status badges to `README.md` for each job
3. Anyone visits the repo or Actions tab to see results

### User Access

- Actions tab: `https://github.com/<org>/<repo>/actions/workflows/nightly_quality.yml`
- Badges visible on the repo landing page (README)

### Cost

- **$0**

### Pros

- Zero maintenance — no dashboard code to maintain
- GitHub provides filtering, logs, re-run buttons natively
- Historical runs are preserved indefinitely in the Actions tab
- Badges auto-update

### Cons

- No unified "at-a-glance" dashboard page
- No coverage visualization (must download artifacts manually)
- Badges only show pass/fail for the entire workflow, not per-job granularity
- Less polished for external stakeholders

---

## Option C: GitHub Actions + Third-Party Dashboard (Codecov, Grafana Cloud, etc.)

### Summary

Use GitHub Actions for CI, but publish metrics to a dedicated dashboard service.

### How It Works

1. Same nightly workflow runs tests + coverage
2. Upload coverage to **Codecov** (free for public repos)
3. Optionally push metrics to **Grafana Cloud** free tier (10k metrics, 50 GB logs)
4. Dashboard lives on the third-party service

### User Access

- Codecov: `https://codecov.io/gh/<org>/<repo>`
- Grafana: custom dashboard URL

### Cost

- **$0** for Codecov (public repos) and Grafana Cloud free tier

### Pros

- Rich historical trends, graphs, and PR annotations (Codecov)
- Professional-looking dashboards with no custom code
- Codecov integrates with PR checks (coverage diff on every PR)
- Grafana supports alerting

### Cons

- External dependency — service outages affect visibility
- Data lives outside your control
- Grafana free tier has retention limits (14 days for logs)
- More complex setup (API keys, secrets)
- Team must adopt another tool/login

---

## Option D: GitHub Actions + GitHub Pages with Historical Trends

### Summary

Enhanced version of Option A. Persist historical results in a JSON file on the `gh-pages` branch and render trend charts in the dashboard.

### How It Works

1. Same nightly workflow
2. Before deploying, fetch previous `history.json` from `gh-pages` branch
3. Append current run results, keep last N entries (e.g., 90 days)
4. Dashboard includes a JavaScript chart (Chart.js or similar) showing pass/fail over time
5. Deploy updated dashboard + history to GitHub Pages

### User Access

- Same as Option A: `https://<org>.github.io/<repo>/`

### Cost

- **$0**

### Pros

- All benefits of Option A, plus historical trend visualization
- Still fully self-contained, no external services
- Stakeholders can see quality trajectory over time

### Cons

- More complex to implement (JSON history management, chart rendering)
- Risk of data loss if `gh-pages` branch is force-pushed
- More JavaScript in the dashboard (maintenance burden)
- Still limited compared to dedicated tools (no PR annotations, no alerting)

---

## Comparison Matrix

| Criteria                | A (Pages)    | B (Badges) | C (3rd Party) | D (Pages+History) |
|-------------------------|--------------|------------|----------------|--------------------|
| Cost                    | Free         | Free       | Free           | Free               |
| Setup effort            | Low          | Minimal    | Medium         | Medium             |
| Maintenance             | Low          | None       | Low            | Medium             |
| Historical trends       | No           | Via Actions tab | Yes       | Yes                |
| Coverage visualization  | Yes          | No (artifact only) | Yes (Codecov) | Yes          |
| External dependency     | No           | No         | Yes            | No                 |
| At-a-glance dashboard   | Yes          | No         | Yes            | Yes                |
| PR integration          | No           | Badges only | Yes (Codecov) | No                 |
| Alerting                | Manual       | None       | Yes (Grafana)  | Manual             |

---

## Recommendation

**Start with Option A** — it's virtualised, free, and low-maintenance.
