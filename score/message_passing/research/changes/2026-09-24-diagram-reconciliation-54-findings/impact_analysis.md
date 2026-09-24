# Message Passing — Impact Analysis: diagram-reconciliation-54-findings

## Validator rules actually enforced (ground truth, read directly from `@score_tooling` specs)

Read `component_sequence.md`, `component_internal_api.md`, `sequence_internal_api.md`,
`component_public_api.md` in full (cached under
`external/score_tooling+/validation/core/docs/specifications/`). Key mechanics that shape the fix:

- **Alias Consistency**: the set of `<<unit>>` aliases in `static_design.puml`
  (`client_connection`, `server_connection`, `qnx_dispatch`, `unix_domain` — **not** `dispatch`,
  which is `<<component>>`, not `<<unit>>`) must exactly equal the set of participant aliases used
  across all sequence diagrams. The literal alias `ExternalEndpoint` is exempt (represents any
  caller/callee outside the modeled units). This exactly explains the 10 `[Naming]` findings:
  4 missing (the real unit aliases never appear) + 6 unexpected (`client_app`, `client_conn`, `os`,
  `server`, `server_app`, `server_conn` — none of which is a unit alias or `ExternalEndpoint`).
- **Interface-Connection Consistency**: every cross-unit sequence call needs the two units bound to
  a shared interface in `static_design.puml` (direction-agnostic); conversely every interface
  connection needs at least one exercising call. Self-calls and any call touching
  `ExternalEndpoint` are excluded entirely.
- **Component Internal API**: every interface declared *nested inside a component/package* in
  `static_design.puml` must be declared (by qualified-name) in `private_api.puml`. Top-level
  interfaces (bound straight to the `<<SEooC>>`) are silently ignored by this check — that's the
  public API validator's job instead.
- **Sequence Internal API**: method names are matched by exact text before `(`, so
  `IServerFactory::Create(...)` extracts as the literal method name `IServerFactory::Create`, not
  `Create` — a `::`-qualified call site will never match a plain `Create()` declaration. Method-name
  consistency and consumer/provider-role checks only apply to cross-unit calls where the two units
  already share an interface (Interface-Connection Consistency is the pre-condition, checked by a
  different validator). Interface Coverage requires every declared internal-API method to be
  exercised somewhere (self-calls count).
- **Component Public API**: match is by bare **name**, not fully-qualified id — so an interface
  nested inside `public_api.puml`'s `namespace score::message_passing { ... }` already matches a
  top-level `static_design.puml` interface of the same bare name; nesting in `public_api.puml` is
  not itself a problem. Any relation type/role from the SEooC to the interface counts.

## Root cause (upward trace)

`static_design.puml` currently declares only ONE fictitious top-level interface for the entire
public surface (`interface "score::message_passing" as message_passing_public_api`) — this name
does not match ANY of the six real interfaces actually declared inside `public_api.puml`
(`IClientConnection`, `IClientFactory`, `IServer`, `IServerFactory`, `IServerConnection`,
`IConnectionHandler`), which is the true, pre-existing root cause of the 2 old `[Interface]`
findings (Finding 4, 2026-09-23) — not a missing `public_api.puml` declaration as originally
guessed, but a wrong/invented name in `static_design.puml`. Separately, `static_design.puml` binds
zero interfaces between its own internal units, and was never reconciled against the sequence
diagrams' (`client_app`/`client_conn`/`os`/`server`/`server_app`/`server_conn`) or
`private_api.puml`'s (`Dispatch.QNX.Client/Server`, etc.) invented naming — both authored
independently, per the 2026-09-23 cycle's Finding 5.

## Real architecture (downward/grounding trace, from code)

- `client_connection` unit ⇒ concrete class `detail::ClientConnection` (`client_connection.h`),
  implements `IClientConnection`, depends on `ISharedResourceEngine` (constructor-injected
  `shared_ptr<ISharedResourceEngine>`).
- `server_connection` unit ⇒ **interface-only** (`unit.implementation = [":common_headers"]`, no
  concrete class of its own) — represents the `IServerConnection` contract in the abstract; the
  concrete realizations (`UnixDomainServer::ServerConnection`, `QnxDispatchServer::ServerConnection`)
  physically live inside the `unix_domain`/`qnx_dispatch` units.
- `unix_domain` unit ⇒ `UnixDomainEngine` (implements `ISharedResourceEngine`),
  `UnixDomainClientFactory`/`UnixDomainServerFactory` (implement `IClientFactory`/`IServerFactory`),
  `UnixDomainServer` (implements `IServer`) + its nested `ServerConnection` (implements
  `IServerConnection`). All in `namespace score::message_passing::detail` /
  `score::message_passing`.
- `qnx_dispatch` unit ⇒ same shape, `QnxDispatchEngine`/`QnxDispatchClientFactory`/
  `QnxDispatchServerFactory`/`QnxDispatchServer` (+ nested `ServerConnection`). Currently
  `unit.implementation = []` (pre-existing, separate backlog item — not touched here).
- The real internal seam between `client_connection`/`server_connection` and the two dispatch
  backends is `ISharedResourceEngine` (`i_shared_resource_engine.h`): `TryOpenClientConnection`,
  `CloseClientConnection`, `SendProtocolMessage`, `ReceiveProtocolMessage`, `RegisterPosixEndpoint`,
  `UnregisterPosixEndpoint`, `EnqueueCommand`, `GetMemoryResource`, `GetLogger`,
  `IsOnCallbackThread`. This interface is declared nowhere in any current diagram — the single
  biggest structural gap `private_api.puml`'s placeholder interfaces were trying (badly) to stand
  in for.

## Reconciliation plan (Step 2/3, mechanical alignment only per Open Question 4)

1. **`static_design.puml`**: replace the single fictitious `message_passing_public_api` top-level
   interface with the six real top-level interfaces (`IClientConnection`, `IClientFactory`,
   `IServer`, `IServerFactory`, `IServerConnection`, `IConnectionHandler`), each bound directly from
   `dependable_element_message_passing` (provided, except `IConnectionHandler` and `os`, which are
   required — the library depends on the *caller* implementing/supplying them). Add one nested
   internal interface, `ISharedResourceEngine` (curated to the 5 methods actually exercised by the
   reconciled sequence diagrams), bound required from `client_connection`/`server_connection` and
   provided from `qnx_dispatch`/`unix_domain`. Also bind `IServerConnection` from `qnx_dispatch`/
   `unix_domain` (they realize it) in addition to `server_connection`, and `IClientFactory`/
   `IServerFactory` from `qnx_dispatch`/`unix_domain` (they realize those too, not `client_connection`
   — the factories are what construct `ClientConnection`, not the other way around).
2. **`public_api.puml`**: add one new top-level (non-namespaced) interface `"OS" as os` with a
   curated method list mirroring `ISharedResourceEngine`'s real signatures (the OS-level contract
   the library requires from its environment is, by construction, the same set of operations
   `ISharedResourceEngine` abstracts) — no other content changes needed, since the six existing
   namespaced interfaces already match the new top-level `static_design.puml` declarations by bare
   name (public API matching is name-only, not fully-qualified).
3. **`private_api.puml`**: full rewrite (Open Question 2) — replace the placeholder
   `Dispatch.QNX.Client/Server`/`Dispatch.UnixDomain.Client/Server` interfaces with the one real,
   shared `ISharedResourceEngine` interface (curated method subset), nested so it resolves against
   `static_design.puml`'s nested declaration.
4. **Sequence diagrams** (all three): rename participants —
   `client_conn`→`client_connection`, `server_conn`→`server_connection`, `client_app`/`server_app`
   →`ExternalEndpoint` (both external actors collapse to the one exempt name — acceptable since the
   validator only reasons about library-internal units, not what's outside them), `server`→ folded
   into `ExternalEndpoint`/`server_connection` calls directly (no separate "server" unit exists —
   `IServer`'s `StartListening`/`StopListening` are realized by `unix_domain`/`qnx_dispatch`, so
   those calls target those units in an `alt` block instead), `os`→ replaced: the literal kernel is
   out of scope for unit-level diagrams; transport hand-off is now shown as
   `client_connection`/`server_connection` calling `unix_domain` **or** `qnx_dispatch` inside an
   `alt Linux (Unix Domain Sockets) / QNX (QNX Dispatch)` block using real `ISharedResourceEngine`
   method names (`SendProtocolMessage`/`ReceiveProtocolMessage`/`TryOpenClientConnection`/
   `RegisterPosixEndpoint`/`CloseClientConnection`), not the invented `[IPC] SEND message`-style
   labels.
5. Strip the `IServerFactory::`/`IClientFactory::` prefixes from `Create()` calls (method-name
   extraction is text-before-`(`, so a `::`-qualified call never matches a plain `Create()`
   declaration) — this alone likely accounts for several of the 29 `[Method]` findings.

## Artifacts to touch

- `software_architectural_design/static_design.puml`
- `software_architectural_design/public_api.puml` (additive: one new top-level interface)
- `software_architectural_design/private_api.puml` (full rewrite)
- `software_architectural_design/server_client_sequence.puml`
- `software_architectural_design/server_client_internal_fault_sequence.puml`
- `software_architectural_design/server_client_os_fault_sequence.puml`

No `BUILD`, `requirements/`, `assumed_system/`, or `safety_analysis/` changes — confirmed no FTA
content references any of these diagrams by name (Step 0).

## Artifacts explicitly NOT touched (and why)

- `client-server.md` — prose design doc, not part of any validated diagram; the sequence-diagram
  content changes here are purely mechanical alignment (Open Question 4), not new design content
  that would need re-describing there.
- `software_unit_design/` — still empty, pre-existing separate backlog item.
- The FTA/control-measure content the sequence diagrams already reference by name
  (`BE_SendQueueExhausted`, `SendBufferArgumentValidation`, etc., in
  `server_client_internal_fault_sequence.puml`) — those references are prose notes, not
  machine-validated links; left as-is since renaming participants/methods doesn't touch that text.

## Validation plan

Iterate with `bazel build //score/message_passing/dependability/software_architectural_design:message_passing_architectural_design`
after each edit round and adjust based on the validator's actual (not hand-simulated) output, per
the established repo practice for this custom `puml_cli` parser/validator suite.
