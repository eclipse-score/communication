# Message Passing — Nice to Haves

Long-lived, shared across all actualization cycles. Possible future improvements noticed in
passing, not (yet) inconsistencies or defects — see `backlog.md` for those.

## From the 2026-08-31 baseline snapshot

- `client-server.md` calls out passing shared-memory region handles between processes (native in
  Unix Domain Socket messaging; `shm_create_handle()` on QNX) as a possible future extension, out
  of scope for the first release.
- `client-server.md` calls out a watchdog-friendly notification interface (paired client-side
  arm / server-side disarm callbacks around `Notify`) as a possible future extension to give
  timing guarantees for notification messages, which today have none.
- `client-server.md` calls out larger shared thread pools serving multiple `Server`/
  `ClientConnection` instances concurrently as a possible future extension beyond the current
  single-thread-or-single-pair-of-threads model.

## From the 2026-09-17 architecture-completeness / FTA-redo discussion

- An informal, "virtual" **system-level FTA** for `message_passing` — root nodes = negated
  `AssumedSystemReq`/`Mitigation` records in `assumed_system/assumed_system_requirements.trlc`
  (currently just `SystemMessagingProtocol` and `SafeState`), reasoned top-down from the system's
  point of view. Explicitly **not** to be wired into the TRLC/`fmea()` graph — see the new optional
  skill `.github/skills/rules-score-safety-analysis-tiered-fta/SKILL.md` for the full rationale (Tier 1
  vs. the existing component-API-level micro-FTAs). Would live as a plain, un-wired sketch under
  `research/` if ever done. Parked here, not started.

## From the 2026-09-22 feature-req-notify-split-and-crossplatform-qm cycle

- Model client-initiated one-way `Send` as `SynchronousBidirectionalCommunication` with an empty
  reply instead of as a distinct one-way capability. Would make the "blocks until the receiving
  side's handler has processed the call" guarantee portable to both backends (today it holds only
  for QNX's synchronous native transport, not for Linux's Unix Domain Socket `write()`), at the
  cost of merging two currently-separate `IClientConnection` API surfaces. Raised by the human as a
  design thought during that cycle; explicitly not directed to be implemented — this cycle only
  fixed requirement wording to stop overclaiming the QNX-only guarantee, not the API/architecture.


