# Message passing knowledge

Start here for current subsystem context. Read [decisions](decisions.md) for intent,
[open items](open-items.md) for known discrepancies, and [validation](validation.md)
for artifact wiring and evidence entry points, only as relevant. Repository commands
and external tooling constraints live in the [repository depot](../../../docs/engineering/seooc-maintenance.md).

These notes are maintained by humans and agents. They describe current understanding;
Git retains their history. Recheck affected sources before editing. Code describes
behavior, TRLC describes obligations, and human decisions describe intent. A
disagreement requires reconciliation, not silently declaring one universally correct.

## Boundary and source map

- Same-host, connection-oriented IPC with one public API and Unix-domain-socket and
  QNX-native-dispatch backends. The intended safety scope is QNX/ASIL B; non-QNX is QM.
  This does not claim achieved qualification. See
  [assumed requirements](../dependability/assumed_system/assumed_system_requirements.trlc)
  and [feature requirements](../dependability/requirements/feature_requirements.trlc).
- The [dependable element](../dependability/BUILD) has `integrity_level = "B"`,
  `maturity = "development"`, and Linux-compatible documentation targets.
- Public contracts: [client connection](../i_client_connection.h),
  [client factory](../i_client_factory.h), [server](../i_server.h),
  [server factory](../i_server_factory.h), [server connection](../i_server_connection.h),
  [handler](../i_connection_handler.h), and [protocol configuration](../service_protocol_config.h).
  Concrete aliases are in [engine](../engine.h), [client_factory](../client_factory.h),
  and [server_factory](../server_factory.h).
- [Architecture](../dependability/software_architectural_design/client-server.md),
  [diagram wiring](../dependability/software_architectural_design/BUILD),
  [component requirements](../dependability/requirements/component_requirements.trlc),
  [safety analysis](../dependability/safety_analysis/BUILD), and [implementation/test wiring](../BUILD)
  are the working sources. Do not duplicate complete record or method inventories here.
- [LoLa messaging](../../mw/com/impl/bindings/lola/messaging/) is one concrete consumer.
  Inspect it when affected, without making the public contract consumer-specific or
  assuming it is the only consumer.

## Architecture and behavior facts

`ClientConnection` is shared through `ISharedResourceEngine`. The backend servers and
their nested connections use concrete engine types; there is no analogous shared
server implementation through that interface. The `server_connection` Bazel unit
represents common headers, not another concrete server. See [client_connection.h](../client_connection.h),
[Unix-domain server](../unix_domain/unix_domain_server.h), [QNX server](../qnx_dispatch/qnx_dispatch_server.h),
and [BUILD](../BUILD).

`score::os` is the external OS abstraction. `ISharedResourceEngine` is an internal
client-side transport abstraction. The OS diagram selects QNX `OsResources` wrappers;
thread/synchronization primitives are deliberately omitted for brevity.

`Send` is one-way but can call transport inline and block. It queues when
`truly_async`, or when `fully_ordered` and a reply is pending. Queue capacity alone
does not choose this behavior. OS acceptance, peer receipt, and handler completion
are different guarantees. `Notify` has a separate asynchronous feature.
`SendWithCallback` is request/reply despite its callback return path; parent
traceability needs correction (MP-01/MP-02). See [implementation](../client_connection.cpp).

`UserData` has non-owning/value alternatives (`void*`, `std::uintptr_t`) and an
owned handler alternative (`score::cpp::pmr::unique_ptr<IConnectionHandler>`).
Client identity is transport-dependent: native QNX and Linux UDS obtain it from the
OS; UDS compiled on QNX supplies zero placeholders. See [server types](../server_types.h)
and [UDS implementation](../unix_domain/unix_domain_server.cpp).

## Working context

Feature objectives, cycle status, acceptance, and investigation history belong in
the relevant `../work/<feature>/` directory, not this depot. Loading knowledge does
not resume any feature. Read only the selected feature's current handoff.

Legacy `research/` and `chat_memory/` are retained historical input, not routine
context or instructions to start work. Their statements may be superseded. Promote
necessary knowledge with source evidence; do not duplicate their chronological logs.
