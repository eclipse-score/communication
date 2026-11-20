# Activity Nested Else/If Resolution

Status: open
Area: tooling
Scope: activity diagrams; tooling pin 3.0.2
Owner: unassigned
Evidence checked: 2026-11-06; source inspection and focused unit-build reproduction

## Finding

A valid nested activity conditional fails resolution when the first statement
of its enclosing `else` branch is another `if`. The activity resolver treats
this as an `elseif` chain and consumes the inner `endif` as the enclosing
conditional's terminator. The remaining outer `endif` then produces
`unexpected statement endif while parsing statement`.

This was reproduced while adding the
[connection-establishment activity](../../../dependability/software_unit_design/client_connection_connect_activity.puml)
to the existing client unit. Giving the inner accepted branch an explicit
action did not resolve the failure.

## Evidence and Workaround

Run from the communication repository root:

```sh
bazel build //score/message_passing:client_connection
```

In the activity view, removing `:Restart(): inspect state_;` from the beginning
of the `else (Restart)` branch reproduces the failure. Keeping that action
before the nested state guard avoids the ambiguity: the resolver sees an
ordinary else-body rather than entering its elseif-chain path. The action
describes the existing state inspection; it does not add a runtime operation
or change the restart guard. The repaired unit build succeeds.

In the pinned tooling source,
`plantuml/parser/puml_resolver/src/activity_diagram/src/activity_resolver.rs`,
`visit_if` chooses `visit_elseif_chain` whenever the statement immediately
after `Else` is `IfStart`, then sets `endif_consumed_by_elseif = true`.
This source-level decision explains the observed extra-terminator error.
No external tooling source was modified.

This issue is separate from the pilot's
[class type-comparison findings](tooling-client-connection-unit-design-types.md).
Activity parsing and real PlantUML rendering remain independent checks; neither
establishes behavioral correspondence with the C++ implementation.

## Next Check

On a tooling update, rerun the nested-conditional variant without the leading
action and check that both conditionals survive resolution with their proper
closing boundaries. An upstream fix should distinguish an explicitly nested
else-body from an elseif chain, not discard an `endif` or flatten the intended
control flow. Until then, retain the explicit state-inspection action.