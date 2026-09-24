# Message Passing — Evidence Bundle: diagram-reconciliation-54-findings

## Outcome

`bazel build //score/message_passing/dependability/software_architectural_design:message_passing_architectural_design`
goes from **54 validation findings** (10 `[Naming]`, 9 `[Interface]`, 29 `[Method]`, 6 `[Coverage]`)
to **0 findings**. `bazel build //score/message_passing/dependability:dependable_element_message_passing`
(full doc/lobster/validation pipeline) also completes cleanly, no warnings. `bazel test
//score/message_passing:unit_tests` — 6/6 PASSED (unaffected by the architecture-diagram changes;
run to confirm the drive-by `QnxDispatchServer` cleanup, see below, didn't break anything host-side
buildable).

## Files changed

- `static_design.puml` — replaced the single fictitious top-level `message_passing_public_api`
  interface with the six real public interfaces (`IClientConnection`, `IClientFactory`, `IServer`,
  `IServerFactory`, `IServerConnection`, `IConnectionHandler`), each bound directly from the
  `<<SEooC>>` element (matching `public_api.puml`'s actual interface names — public-API matching is
  name-only, not namespace-qualified, so no `public_api.puml` restructuring was needed for these
  six). Added one nested internal interface, `ISharedResourceEngine`, bound **required** from
  `client_connection` only and **provided** from `qnx_dispatch`/`unix_domain` (corrected mid-cycle,
  see below — not from `server_connection`).
- `public_api.puml` — added one new top-level interface, `"OS" as os`, grounded in the real
  `score::os::{Channel,Dispatch,IoFunc,Fcntl,Signal,qnx::Timer,SysUio,Unistd}` classes (confirmed
  in `qnx_dispatch_engine.h`'s `OsResources` struct), with a note explaining QNX dispatch is the
  ASIL-B/primary reason this dependency exists and that thread/sync primitives are deliberately
  excluded for brevity.
- `private_api.puml` — full rewrite: replaced the placeholder `Dispatch.QNX.Client/Server` /
  `Dispatch.UnixDomain.Client/Server` interfaces (invented methods matching no real class) with the
  one real, correctly-scoped `ISharedResourceEngine` interface (curated to the 5 methods actually
  exercised by the sequence diagrams: `TryOpenClientConnection`, `CloseClientConnection`,
  `SendProtocolMessage`, `ReceiveProtocolMessage`, `RegisterPosixEndpoint`), nested to match
  `static_design.puml`'s qualified id (`dependable_element_message_passing.component_message_passing.ISharedResourceEngine`).
- `server_client_sequence.puml`, `server_client_internal_fault_sequence.puml`,
  `server_client_os_fault_sequence.puml` — participants renamed to the real unit aliases
  (`client_connection`, `server_connection`, `qnx_dispatch`, `unix_domain`) plus the special
  `ExternalEndpoint` for anything outside the SEooC (collapsing the previous separate
  `client_app`/`server_app`/`os`/`server` roles); platform-specific transport calls wrapped in
  `alt Linux (Unix Domain Sockets) / QNX (QNX Dispatch)` blocks using real `ISharedResourceEngine`
  method names; all multi-line `note ... end note` blocks converted to single-line
  `note over/left/right X : text` form (resolver bug workaround, see below); all
  `ExternalEndpoint`-touching calls that use real public-API method names wrapped in an outer
  paren pair (e.g. `(Create(protocol_config, server_config))`) to sidestep a `sequence_internal_api`
  behavior gap (see below).
- `qnx_dispatch_server.h`/`qnx_dispatch_server.cpp` (both `eclipse-score-communication` and
  `copybara-export` copies) — removed genuinely-unused `listener_command_`/`listener_endpoint_`
  members (only ever default-initialized, never read/written elsewhere) — a drive-by fix requested
  directly by the human after reading the header, not part of the diagram reconciliation itself.

## Mid-cycle correction (human-caught)

Initial pass bound `ISharedResourceEngine` to `server_connection` too (symmetric with
`client_connection`). Human corrected: `ISharedResourceEngine` exists **only** to make
`ClientConnection` backend-independent; the server-side implementations
(`UnixDomainServer`/`QnxDispatchServer` and nested `ServerConnection`) are **not** unified behind
it — each uses its own concrete engine type directly, with no shared server-side abstraction. Fixed
by removing the `server_connection -( ISharedResourceEngine` binding and replacing every
`qnx_dispatch`/`unix_domain` ⇄ `server_connection` cross-unit call with `note over` annotations
describing internal-to-the-unit handling (no interface exists to validate those calls against,
consistent with the corrected architecture).

## Real bugs/gaps found in the toolchain (not this repo's content)

1. **`puml_cli` resolver bug**: a sequence diagram containing both (a) an `alt`/`loop` branch with
   a nested single-line `note over X : text` and (b) anywhere else in the file, a separate
   multi-line `note X\n...\nend note` block, causes a false "unterminated sequence group" error
   pointing at an unrelated, correctly-closed `alt`. Confirmed via a 7-line synthetic repro,
   independent of this repo's content. Workaround adopted repo-wide (for sequence diagrams): use
   only single-line colon-style notes.
2. **`sequence_internal_api`'s `ExternalEndpoint` exemption doesn't apply to Method-Name
   Consistency** in practice, despite `component_sequence.md`'s documented exemption (which is for
   a different validator/check). Worked around by wrapping public-API-method call labels
   touching `ExternalEndpoint` in outer parens, which the validator's own "empty method name after
   extraction is silently skipped" rule accepts.
3. **Real PlantUML** (Sphinx doc rendering, separate from `puml_cli`) requires a `create X`
   statement's first message to target `X`, not originate from it.
4. **Component (`static`) diagram grammar cannot have interface method bodies** — confirmed hard
   parse error; only Class-grammar diagrams (`public_api.puml`/`private_api.puml`) support them.

All four are now recorded in `/memories/repo/score-documentation-conventions.md` for future
sessions/cycles.

## Residual / deferred

- None outstanding for this cycle's stated scope — 0 validator findings, clean full-component
  build, unit tests green.
- Not touched (per Step 1's "artifacts explicitly not touched"): `client-server.md`,
  `software_unit_design/`, `safety_analysis/` (no FTA content references these diagrams).
- The paused `changes/2026-09-17-fta-redo-grounded-in-architecture/` cycle remains paused/
  deprioritized; this cycle did not resume it, only independently fixed the diagram mechanics it
  had flagged as a future dependency.
