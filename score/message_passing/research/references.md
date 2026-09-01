# Message Passing — References

Long-lived, shared across all actualization cycles. Capture pointers to external code, docs, or
artifacts that inform impact analysis; do not fold cycle-specific detail in here (that belongs in
`changes/<cycle>/`).

## Known consumers (orientation evidence only, not exhaustive)

- **`score/mw/com/impl/bindings/lola/messaging/`** (LoLa binding of `mw::com`) is one known,
  concrete consumer of `score/message_passing/`'s client/server connection API, used for LoLa
  method calls:
  - [message_passing_service.h](../../mw/com/impl/bindings/lola/messaging/message_passing_service.h) /
    [message_passing_service.cpp](../../mw/com/impl/bindings/lola/messaging/message_passing_service.cpp)
  - [message_passing_client_cache.h](../../mw/com/impl/bindings/lola/messaging/message_passing_client_cache.h) /
    [message_passing_client_cache.cpp](../../mw/com/impl/bindings/lola/messaging/message_passing_client_cache.cpp)

  This is recorded purely as orientation evidence for future impact analyses ("does a change
  affect this consumer's usage pattern?"). `message_passing`'s public API and architecture must
  stay consumer-agnostic — this one consumer's needs must never drive the baseline snapshot's
  content or be assumed to be the only consumer. Do not go looking for additional specific
  consumers beyond what already exists in the repository today.

## Related lifecycle skills

- `.github/skills/rules-score-actualize/SKILL.md` — the process this `research/` directory
  belongs to.
- `.github/skills/rules-score/SKILL.md`, `.github/skills/score-requirements/SKILL.md`,
  `.github/skills/score-architecture/SKILL.md`, `.github/skills/score-safety-analysis/SKILL.md`,
  `.github/skills/score-testing/SKILL.md` — the mechanical skills `rules-score-actualize`
  sequences per cycle.
