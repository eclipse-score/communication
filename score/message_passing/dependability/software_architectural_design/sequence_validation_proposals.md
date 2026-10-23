# Integration Sequence Validation Proposals

## Purpose And Scope

This document proposes improvements for the team maintaining `score_tooling`.
It does not change that tooling or prescribe its implementation. The SEooC and
tooling can evolve together while `maturity = "development"` is in use.

The concrete example is
[the integration pilot](client_server_async_integration.puml): setup and one
asynchronous request/reply through the OS-independent public API. Existing
QNX/Linux sequences remain implementation views; the pilot does not replace them.
Disconnect teardown, notifications, connection rejection/retry, synchronous
request/reply, and the per-connection-handler alternative are outside this pilot.

## Pilot Modeling Conventions

- Application actors have distinct aliases and `<<external>>` stereotypes. They
  are not library units and must not be added to the certified unit hierarchy.
- Library lifelines are API object instances: `client : IClientConnection`,
  `server : IServer`, and `connection : IServerConnection`. A connection handle
  is not interchangeable with the listening server object.
- Public API contracts include both library-provided operations and
  application-provided callbacks. In [the static view](static_design.puml), the
  latter are required interfaces. In [the public API view](public_api.puml),
  `<<callback>>` and `operator()` describe existing `score::cpp::callback` type
  aliases, not new C++ virtual interfaces or new callback method names.
- Callback calls are solid arrows to the application that supplied the callable.
  Dashed arrows return from a C++ call. `IPC:` labels identify asynchronous
  transport messages, not calls to an invented C++ interface. This label is a
  provisional documentation convention, not an existing checker capability.
- The drawing shows one permitted interleaving. Vertical placement does not
  establish an additional cross-thread guarantee. In particular, a completion
  callback is not promised to occur after `SendWithCallback()` returns.

## Current Tooling Evidence

Observed on 2026-10-23 with the repository's resolved `@score_tooling` dependency:

- Before the pilot, validation reported **9 findings**: external-participant
  naming and interface-connection findings in the existing sequences.
- Adding the callback declarations and signatures to the public API alone did
  not add findings. The parser accepts the `operator()` notation.
- With the pilot registered in `architectural_design.dynamic`, validation reports
  **37 findings**, including **28 additional findings**: 3 Naming, 5 Interface,
  and 20 Method. There are no additional public/internal API declaration errors.
- The build exits successfully because the target is in development maturity;
  that is not evidence that the new public interactions have been checked.

The additional findings are explained by the current model, rather than by
missing implementation units:

| Current behavior | Pilot evidence | Needed distinction |
| --- | --- | --- |
| Every non-reserved participant must name an internal unit | `client_api`, `server_api`, `server_connection_api` are rejected | Public API object instance versus internal unit |
| Only the literal alias `ExternalEndpoint` is special | Distinct application actors are rejected | Explicitly declared external actors |
| Cross-lifeline messages require a shared internal interface | Application calls and `IPC:` messages are rejected | Public boundary call versus transport message versus internal call |
| Sequence method validation receives only the internal API index | `StartListening`, `Start`, `SendWithCallback`, `Reply`, and callbacks are rejected | Public contract and callback invocation resolution |
| Dashed responses are also processed as method names | `void`, `expected_blank<Error>{}`, and returned `UserData` are rejected | C++ return value versus function call |

The relevant upstream entry points are
`validation/core/src/profiles/architectural_design.rs`,
`validation/core/src/validators/component_sequence_validator.rs`, and
`validation/core/src/validators/sequence_internal_api_validator.rs` in
`@score_tooling`. The architectural-design profile currently passes public API
information to the component/public-API validator, but not to sequence method
validation.

Reproduce from the repository root:

```sh
bazel build //score/message_passing/dependability/software_architectural_design:message_passing_architectural_design
cat bazel-bin/score/message_passing/dependability/software_architectural_design/message_passing_architectural_design/validation.log
```

## Proposed Capabilities

### 1. Explicit External Actors And Integration Views

Recognize multiple distinct external actors through explicit metadata, such as
the pilot's `<<external>>`, rather than a single magic alias. Preserve their
identities in parsing, validation, and rendering. External actors should be
exempt from internal-unit membership checks, not from public-contract checks.

Allow a sequence to describe an integration/API view rather than a unit view.
Resolve API object lifelines using their declared types and the public API
index. Multiple instances of one type must remain distinct lifelines. Keep
internal-unit naming and completeness checks for implementation views; do not
require an integrator-facing sequence to expose all implementation units.

### 2. Boundary Calls And Callable Bindings

Check application-to-library calls against the receiving object's public
interface, including the required/provided direction at the SEooC boundary.
Finding the same method name on another interface must not satisfy that check.
For example, `Reply()` is valid on `IServerConnection`, not on `IServer`.

Resolve callable values through registration and parameter types. The pilot
requires these bindings:

| Registration | Callable value | Invocation contract |
| --- | --- | --- |
| `IClientConnection.Start` | `state_callback` | `IClientConnection::StateCallback::operator()` |
| `IClientConnection.Start` | `notify_callback` | `IClientConnection::NotifyCallback::operator()` |
| `IClientConnection.SendWithCallback` | `reply_callback`, supplied as parameter `callback` | `IClientConnection::ReplyCallback::operator()` |
| `IServer.StartListening` | `connect_callback` | `ConnectCallback::operator()` |
| `IServer.StartListening` | `disconnect_callback` | `DisconnectCallback::operator()` |
| `IServer.StartListening` | `sent_callback`, `sent_with_reply_callback` | `MessageCallback::operator()` |

Support explicit binding annotations if types and registration cannot be
resolved unambiguously from the sequence. Do not require the public API to
invent methods literally named `state_callback` or `sent_with_reply_callback`.
The callback's application owner supplies the callable; the library consumes it.

Preserve callable identity when normalizing `operator()`. A call to one
callback must not count as exercising all callback contracts. Any public-API
coverage policy should apply to a declared scenario set, not demand every
operation in this deliberately narrow pilot. Argument and result checks should
use normalized types when the diagram supplies enough type information.

### 3. Distinguish Calls, Returns, And Transport Messages

Preserve interaction kind in the sequence model. Do not treat returned values
as method names, and do not require a C++ shared-interface connection for an
explicitly identified IPC message. The IPC reply is still a transport message,
even though a dashed arrow is used to draw it.

The tooling team should choose a supported annotation for transport messages;
the pilot's `IPC:` text merely makes the intended distinction explicit today.
Callbacks remain ordinary callable invocations for signature/direction checks,
even when execution happens on an unspecified thread.

### 4. Report What Was Actually Checked

Distinguish successfully checked calls from unsupported or unresolved
interactions. Do not silently equate an empty extracted method name with a
verified interaction. Existing outer-parenthesis labels bypass method checking;
the pilot intentionally does not use that workaround or duplicate public
operations into the private API.

Consistent API models do not prove callback scheduling, lifetime safety, or
message delivery. Those need source review and behavioral tests. The validator
should not infer guarantees from one sequence's vertical ordering.

## Suggested Acceptance Cases

| Case | Expected result |
| --- | --- |
| Two distinctly named external application actors | Both are accepted and remain separate lifelines |
| Two client objects implementing `IClientConnection` | Both resolve to the public contract without becoming the same object |
| Registered `state_callback(State::kReady)` called by the library | Resolves to the required `StateCallback` contract |
| `Reply(reply)` on the connection passed to the connect callback | Resolves to `IServerConnection.Reply` |
| Dashed `void`, `expected` and `UserData` returns | No method lookup or method-coverage contribution |
| Explicit IPC request/reply messages | Not mistaken for internal C++ calls |
| Misspelled or unbound `state_callbak(...)` | Rejected as an unresolved callable |
| Application invokes its state callback as a library-provided operation | Rejected for incorrect contract ownership/direction |
| `Reply(reply)` addressed to `server : IServer` | Rejected even though another public interface declares `Reply` |
| Reply callback receives a state enum instead of its expected result type | Rejected when type information is available |
| One `operator()` invocation used to claim every callback is covered | Rejected or reported as incomplete coverage |
| Existing internal call with a missing interface or reversed role | Still rejected by the existing internal checks |

A possible PR sequence is external-actor recognition and return classification,
then API-object/boundary validation, then callable binding and coverage. The
specific API and syntax remain decisions for the tooling owners.

## Behavioral Grounding

The pilot was checked against [the client contract](../../i_client_connection.h),
[client configuration](../../i_client_factory.h),
[the server contract](../../i_server.h),
[callback types](../../server_types.h), and
[the connection contract](../../i_server_connection.h), with code-path checks in
[ClientConnection](../../client_connection.cpp) and
[QNX dispatch server](../../qnx_dispatch/qnx_dispatch_server.cpp):

- `Start`/`DoRestart` invoke the starting-state callback synchronously. Other
  state-callback ordering and thread guarantees come from the public contract.
- `SendWithCallback` rejects an oversized message before queuing it. In the
  chosen `truly_async` configuration, accepted requests use the background send
  queue; local acceptance is not delivery confirmation.
- `ProcessInputEvent`, `ProcessSendQueueUnderLock`, and `SwitchToStopState`
  deliver reply data or an asynchronous error through the supplied callback.
- `ProcessConnect` obtains `UserData` from the connect callback.
  `ServerConnection::ProcessInput` dispatches a request either to the registered
  callback or the handler stored in `UserData`.
- Returning success from the request callback is separate from calling
  `IServerConnection::Reply`; the callback receives the handle used for that call.

No requirements, safety-analysis records, implementation code, or shared
tooling are changed by this pilot.