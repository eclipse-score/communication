# release-notes skill

Create complete, significance-focused release notes with traceable links for improvements, bug fixes, and known issues.

## Requirements

- Attach the release template file to the prompt (for this repository: `.github/RELEASE_TEMPLATE.md`).
- Provide the target release tag and optionally the origin (previous) release tag.
- Use **Claude Haiku 4.5** for this skill.

Prompt with the following command and attach the release template file:
```
/skill:release-notes for <version-tag>
```

## Notes

- The skill expects to preserve the template section structure.
- Every improvement and bug-fix item should include traceable PR links.
- Known issues should include open issue links and compatibility/workaround context.

