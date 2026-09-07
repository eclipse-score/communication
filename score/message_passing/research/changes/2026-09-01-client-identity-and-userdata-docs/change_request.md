# Message Passing — Change Request: client-identity-and-userdata-docs

## Trigger

Continuing directly from the Cycle 0 baseline snapshot (`research/problem_statement.md`), the
human reviewed two open `TODO: TBD` markers in
`dependability/software_architectural_design/client-server.md` against the actual code and
supplied the missing facts:

1. The `UserData` object format is **already decided and implemented**: `server_types.h` defines
   `UserData = std::variant<void*, std::uintptr_t, score::cpp::pmr::unique_ptr<IConnectionHandler>>`.
   `client-server.md` still says "The format of the object is still TODO: TBD; it looks like it can
   be a variant of...".
2. The `GetClientIdentity()` result is **already decided and implemented**: `server_types.h`
   defines `struct ClientIdentity { pid_t pid; uid_t uid; gid_t gid; }` — the client's PID, UID,
   and primary GID, assuming a POSIX-compatible OS. `client-server.md` still says "...returns
   information that can be useful for access control (TODO: TBD)".

In the same message, the human also raised a related, new customer-facing need and asked for a
requirements-wording cleanup:

3. **New need**: customers of `message_passing` want the server side to be able to identify a
   client by PID (unique only among concurrently running processes; a PID can be reused for a
   different process over time) and, when the integrating system dedicates a UID per client
   process, by UID. In addition, the client's UID and/or primary GID should be usable for
   server-side access control decisions. This capability already exists in the code
   (`IServerConnection::GetClientIdentity()` / `ClientIdentity`), but no formal `FeatReq`/`CompReq`
   currently promises it, and no `CompReq` at all currently mandates the `GetClientIdentity` API
   (found during impact analysis, see below).
4. **Safety/transport nuance**: Unix Domain Sockets are not ASIL B-compliant on QNX (the ASIL-B
   path on QNX is QNX-native message passing, per the existing `SafetyCertifiedTransportMechanismUnderQNX`
   requirement); ASIL is not currently a concern on Linux. The Unix Domain Sockets backend may be
   used on QNX for internal testing purposes only, and — confirmed in
   `unix_domain/unix_domain_server.cpp` (`#ifdef __QNX__ // no support for SO_PEERCRED on QNX`) —
   it does not provide real client PID/UID/GID there (all three fields are reported as `0`).
5. **Wording cleanup**: `external_component_requirements.trlc`'s `OSProvidedSenderIdentity` and
   `UnforgableSenderIdentity` records use "sender" where the component's established terminology
   (see `research/problem_statement.md`) is "client" / "Client Connection" / "Server Connection".
   The human asked to reframe them so identification happens **once per Server Connection**, after
   which message passing guarantees the **integrity of the connection itself**, rather than
   implying per-message re-verification.

## Classification

Mixed trigger, three parts (Core Principle 5):

- **(1) and (2) — "wrong today"**: `client-server.md` is stale documentation drift. The facts were
  decided and implemented in code; the architecture doc's `TODO: TBD` markers were simply never
  updated to match. This is a defect in the frozen doc, not a new decision.
- **(3) — new need, with an incidentally-discovered pre-existing gap**: formalizing
  PID/UID/GID-based client identification for access control is new content (nothing was "wrong"
  before — the capability existed informally in code but was never promised as a requirement).
  While tracing this, the downward trace also surfaced that `IServerConnection::GetClientIdentity`
  — unlike `Reply`/`Notify`/`GetUserData`'s siblings `IServerConnectionReplyAPI`/
  `IServerConnectionNotifyAPI` — has **no** matching API-level `CompReq` at all. That omission is
  closed here since it is the direct component-level counterpart of the new feature being added;
  it is not itself a separately new ask, but the natural target layer for the new content.
- **(4) — orientation fact, not (yet) acted on as a requirement/AoU change**: this is safety-
  relevant information about a **test-only backend limitation**, not a claim that any existing
  frozen requirement is wrong today. `SafetyCertifiedTransportMechanismUnderQNX` already correctly
  mandates QNX-native messaging for the QNX ASIL-B path, so the frozen requirement text is
  consistent with the new information. See "Stated scope" below for how this is captured (a `note`
  and a backlog entry, not a new `AoU`/FTA — see Open Questions).
- **(5) — "wrong today"**: a wording/terminology defect in two existing `CompReq` records that
  never traced correctly to the component's established vocabulary.

## Stated scope

Per the human's message, taken at face value (impact analysis in `impact_analysis.md` may reveal a
different/larger true scope):

- Update `client-server.md`'s two `TODO: TBD` passages to state the already-decided facts.
- Reword `OSProvidedSenderIdentity` / `UnforgableSenderIdentity` for consistent "client"
  terminology and to reframe identification as a one-time, per-Server-Connection act followed by a
  connection-integrity guarantee.
- Add formal requirement(s) capturing: identification by PID (with its concurrency-only/reuse-over-
  time caveat), identification by UID (when the integrator dedicates one per client process), and
  UID/GID usable for server-side access control.
- Capture the UDS-on-QNX-is-test-only-and-lacks-real-identity nuance so it is not lost, without
  necessarily expanding into a full new `AoU`/FTA cycle (see Open Questions).

## Open Questions

1. **`TransportMechanismOnLinux` currently carries `safety = ScoreReq.Asil.B`.** The human's
   phrase "we don't yet care about ASIL on Linux" could mean this classification should be
   lowered (e.g. to `QM`) or annotated with a note/rationale. This is a safety-classification
   change and is **not** made in this cycle without explicit confirmation — flagged here instead
   (score-requirements guidance: "confirm the safety level rather than assuming it"). Left
   untouched pending human decision.
2. **Should the UDS-on-QNX/test-only limitation become a proper `AoU` wired into the
   `ConnectionContextDataWrong` FTA** (`fta_connection_context_data_wrong.puml`), per
   `score-safety-analysis`'s normal AoU pattern? Doing that fully is a safety-analysis activity in
   its own right (new `$BasicEvent`, matching `AoU` record, FTA edit) and was judged out of the
   minimal scope of *this* cycle (which was about docs/requirements wording, not FMEA authoring).
   For now the fact is captured as a `note` on the new `CompReq` and as a `backlog.md` entry for a
   dedicated future safety-analysis cycle. Flagging for confirmation that this scoping choice is
   acceptable.
3. `IServerConnection::GetUserData()` has the same gap as `GetClientIdentity()` did (no matching
   API `CompReq`). Left as a `backlog.md` entry, not fixed here, since it is not part of what was
   asked. Flagging in case the human wants it folded into this cycle instead of deferred.
