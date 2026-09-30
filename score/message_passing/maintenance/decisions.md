# Message passing decisions and intent

Current decisions and rationale, not implementation certification. Product changes
must reconcile these with authored contracts and observed behavior. Unsettled
candidate solutions belong in the relevant feature's cycle.

## Requirement level and platform scope

Assumed-system requirements express capabilities assessed by an integrator from
outside: communication model, one-way and request/reply interaction, production-error
and integration-misconfiguration detectability, platform support, and access control.
Method names, queues, and internal failure handling belong below that level.
See [dependability conventions](../dependability/README.md) and
[assumed requirements](../dependability/assumed_system/assumed_system_requirements.trlc).

Use precise behavior descriptions; do not treat “safe-silent” as an established
standards term without a source. No standards-compliance claim follows from these notes.

QNX native dispatch is the intended safety-relevant implementation; Linux/non-QNX
is QM. UDS-on-QNX's zero identity placeholders are not an authentication guarantee.
An integration obligation addressing this remains unresolved.

## Public API allocation

`ClientInterface` supplies the parent for seven client lifecycle/state requirements.
Factory creation stays under `OSIndependentAPI`; engine-sharing requirements stay
under `SingletonFreeImplementation`. `GetDefaultOsResources` is an implementation
helper, not an independent obligation merely because it appears in an API diagram.
Keep QNX engine resource injection distinct from shared-client `ISharedResourceEngine`
injection. See [component requirements](../dependability/requirements/component_requirements.trlc).

## Safety intent requiring artifact reconciliation

The following human-provided intent is not a claim that existing controls or code
already implement it:

1. Report detected errors through an API return/error channel when one exists.
2. Some errors lack a reporting path; establish actual behavior at each failure point.
3. Preserve ordering subject to the configured ordering rules.
4. Where send failure cannot be reported, silence until disconnect/reconnect may be
   the intended fallback; verify the specific path before claiming an implemented control.
5. Isolate one connection's failure from other connections.
6. Termination can be justified when isolation cannot be guaranteed, subject to the
   startup/operation distinction below.
7. Mitigate same-process caller misuse where reasonable; AoUs cover real residual
   integration obligations, not every imaginable misuse.
8. Contain IPC peer misuse with an explicitly determined response.
9. An AoU and a real partial mitigation can coexist. A timing obligation does not
   erase the partial benefit of actual queued/background dispatch.

Startup resource failure can justify termination more readily than a failure to
start new activity during operation. Preserve existing activity where possible.
Exhausting a configured memory resource can still terminate; boundaries need
per-path review. No external-supervisor termination AoU has been agreed.
Preallocation intent is not proof of implemented server preallocation (MP-06).

Peer containment is unresolved: some protocol errors drop a connection, an unhandled
notification can be ignored, and other paths assume callbacks exist. No uniform rule
is established. Check proposed OS-error controls against ignored return values (MP-04).

## FTA method

The optional [tiered-FTA lens](../../../.github/skills/rules-score-safety-analysis-tiered-fta/SKILL.md)
is selected for this subsystem. Any system-level tree is an optional informal aid;
it is not an input to the formal failure-mode graph.

Ground ASIL-B analysis directly in shared code and the QNX dispatch backend.
A Linux/UDS observation does not establish QNX containment. For each cause, establish
propagation, detection/control, residual effect, and the integration obligation.
Partial mitigation is not a full guarantee.

`TryConnect` implements retry with capped delay and terminal-error classification;
a capped delay does not bound retry count. Names such as `ServerHealthCheck` and
`ClientRetryPolicy` are not proof of independent mechanisms. `Restart()` is void
and returns without restarting when not stopped; it does not return `EINVAL`.
See [client_connection.cpp](../client_connection.cpp).

## Decision provenance

Human decisions were extracted from revision
`b8bbca3218f171d07e6b7577e4005bea26aa1479`. Use `git show <revision>:<path>`
for the specific evidence if clarification is needed:

- `score/message_passing/research/safety_concept_notes.md`
- `score/message_passing/research/changes/2026-09-19-assumed-system-requirements-rewrite/`
- `score/message_passing/research/changes/2026-09-23-public-api-diagram-requirements-review/`
- `score/message_passing/research/changes/2026-09-17-fta-redo-grounded-in-architecture/next_steps.md`

These references establish provenance; their old task lists are not current
instructions. Read them for a specific unresolved question, not on every cycle.
