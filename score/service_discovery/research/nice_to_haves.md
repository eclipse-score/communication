# Nice to Haves

Requests toward *other* components/teams that are out of scope for this SEooC. Captured, not
acted on.

- **`score/message_passing/` chunking/fragmentation support.** Today, applications built on
  `message_passing` (including SD) must handle "does this batch fit in one message?" entirely
  themselves — the transport has no built-in support for splitting a logical payload across
  multiple physical messages, or for negotiating a larger-than-configured message on demand. If
  more than one `message_passing` client ends up needing this (SD's batching need is one example),
  it may be worth `message_passing` offering a generic multi-message batch primitive instead of
  every client re-inventing it. Not a blocker for SD — SD can implement its own bounded
  splitting/batching strategy regardless — but worth raising with the `message_passing` owners.
- **Shared UID/ACL policy source of truth.** Both the existing shared-memory/UID/ACL mechanism in
  `score/mw/com` and the new daemon's static authorization policy (Open Question 7) need
  essentially the same "which UID may offer which service-instance" data. If the LoLa
  configuration model already captures this, it would be preferable for the daemon to consume the
  same generated artifact rather than a second, hand-authored policy file — a request toward
  whoever owns `mw_com_config.json`/`impl/configuration/` schema evolution, not something to design
  here.
