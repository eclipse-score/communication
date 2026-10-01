# Releasing

This document describes how to create a release of the communication module.

## Overview

1. Run the automated release workflow (creates a draft release).
2. Create the release notes with the `create-release-notes` skill and put them into the draft release.
3. Publish the release. The release postprocessing workflow then adds the new version to the issue template (via a PR).

## 1. Run the automated release workflow

The workflow [`automated_release.yml`](.github/workflows/automated_release.yml) ("Automated Release Process") is triggered manually.

1. Go to **Actions → Automated Release Process → Run workflow**.
2. Select the branch/commit to release (the release is created for the commit the workflow runs on).
3. Enter the `release_tag`, e.g. `v0.6.0`.

The workflow:
- runs the host quality checks, the QNX build and test, the nightly quality jobs (static analysis, CodeQL, coverage) and the Sphinx documentation build,
- only if all of these pass, creates a **draft** release for the tag. Its body is the unfilled [`.github/RELEASE_TEMPLATE.md`](.github/RELEASE_TEMPLATE.md),
- uploads the quality reports and the documentation archive to the draft release.

If the uploads fail, the draft release is deleted again. If a check fails, no draft release is created.

## 2. Create the release notes

Use the `create-release-notes` skill (see [`.github/skills/create-release-notes`](.github/skills/create-release-notes/SKILL.md)) with a coding agent such as GitHub Copilot CLI, for example:

```
/create-release-notes for v0.6.0
```

The skill:
- determines the range from the previous release tag to the release commit,
- collects PRs, open issues, compatibility restrictions and local patches,
- reviews the public API surface lock (`score/mw/com/api_surface.lock.json`) and documents API changes in the upgrade instructions,
- fills in [`.github/RELEASE_TEMPLATE.md`](.github/RELEASE_TEMPLATE.md) and asks you to review the result.

Review the generated notes carefully and request changes until they are correct. After your confirmation, the skill writes them into the draft release. If no draft release exists, it saves the notes to `docs/release-notes/<release-tag>.md` instead.

## 3. Publish the release

Check the draft release (notes and attached artifacts), then publish it manually on GitHub. Publishing triggers the postprocessing workflow (step 4).

## 4. Postprocessing (automatic)

When the release is published, the workflow [`release_postprocessing.yml`](.github/workflows/release_postprocessing.yml) runs automatically. It opens a PR that adds the new version to the `Affected Version` dropdown in [`.github/ISSUE_TEMPLATE/1-bugfix.yml`](.github/ISSUE_TEMPLATE/1-bugfix.yml) (using [ShaMan123/gha-populate-form-version](https://github.com/ShaMan123/gha-populate-form-version)). Review and merge that PR.

The workflow can also be started manually (**Actions → Release Postprocessing → Run workflow**). Further postprocessing steps should be added to this workflow.
