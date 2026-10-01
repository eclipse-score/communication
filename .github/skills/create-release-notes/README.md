# create-release-notes skill

Create complete, significance-focused release notes with traceable links for improvements, bug fixes, and known issues.

## Requirements

- Attach the release template file to the prompt (for this repository: `.github/RELEASE_TEMPLATE.md`).
- Provide the target release tag and optionally the origin (previous) release tag.
- Use **Claude Sonnet 5.5** or a more powerful AI model for this skill.

Prompt with the following command and attach the release template file:
```
/skill:create-release-notes for <version-tag>
```

## Notes

- The skill expects to preserve the template section structure.
- Every improvement and bug-fix item should include traceable PR links.
- Known issues should include open issue links and compatibility/workaround context.

## Review and publication

- After drafting the release notes, the skill presents them to you and asks whether they are fine or whether changes are needed. This repeats until you confirm they are fine.
- Once confirmed, the skill searches GitHub for a **draft** release matching the given release tag:
  - If found, it updates that draft release's body with the final notes.
  - If not found, it tells you the notes could not be pushed to GitHub, and only then saves them in-repo at `docs/release-notes/<release-tag>.md` as a fallback.

## Progress tracking

- Internally, the skill tracks the workflow's major phases (scoping, evidence gathering, issue/compatibility sweeps, classification, drafting, verification, review loop, publication) as todos in the session database, with dependencies between them, so nothing is silently skipped across a long session. This is internal bookkeeping; you will see plain-language progress updates, not raw todo/SQL details.

