# Client Connection Unit Design Type Comparison Limits

Status: open
Area: tooling
Scope: common-client class comparison; tooling pin 3.0.2
Evidence checked: 2026-11-13; focused unit build and generated validation report

## Finding

The [class view](../../../dependability/software_unit_design/client_connection_class_diagram.puml)
uses declaration-level C++ type names. The unit's class validator reports 12
type-representation differences, while its build succeeds with explicit
development maturity. Before this pilot the same wrapper built successfully
but reported `SKIPPED` because there were no design-class inputs.
The earlier baseline had 11 findings. The original `const ReplyCallback*` owner
added one callback-alias expansion difference. The owner is now
`const ReplySlotToken*`, independent of callback moves; its remaining difference
is qualification of the nested token type during C++ extraction. Its pointee
const remains meaningful; this is not permission to remove it.
The two-helper extraction retains 12 representation findings. Queue fields and
the owner pointer are now declared on `ClientSendQueue`; descriptor retirement
is declared on `ClientConnectionLifetime`. The unit remains in development maturity.

The initial view also modeled `TimedCommandQueue` because both classes shared
the original compilation target, and produced 16 findings. That queue belongs
to the engine's client support, not the `ClientConnection` design. Removing it
from this view removed
five out-of-scope findings; no tooling defect was fixed by that scope correction.

The client unit now selects `message_passing_client_connection` for extraction;
engine unit inputs select their backend library and the separate
`message_passing_timed_command_queue`. This library split does not resolve the
type-representation limitations below.

The findings are presently assessed as representation limitations, not observed
runtime defects. Missing entities, inheritance, member visibility, and method
presence were not reported. This is not complete behavioral verification or
review acceptance of the design.

## Reproduction

Run from the communication repository root:

```sh
bazel build //score/message_passing:client_connection
cat bazel-bin/score/message_passing/client_connection/validation.log
```

The checked report begins `FAILED (12 error(s))`; development maturity makes
these findings non-blocking without hiding the report. `unit()` defaults to
release maturity, independently of the enclosing DE's development setting.
Do not assume that a successful `unit_design` target build alone runs the
implementation comparison; the owning unit does that.

## Representation Differences

| Count | Surface | Observed difference |
| --- | --- | --- |
| 12 | Field types | Unqualified nested types, including the reply-slot token, become qualified; callback aliases expand to `move_only_function`; PMR vector spellings become `vector`. |

The declaration anchors are [ClientConnection](../../../client_connection.h),
[ClientSendQueue](../../../client_send_queue.h), and
[ClientConnectionLifetime](../../../client_connection_lifetime.h)
and the callback aliases in [IClientConnection](../../../i_client_connection.h).

## Earlier Queue Findings

The original view's five additional findings were a qualified queue field type,
the expansion of `ProcessQueue`'s `TimePoint` return alias, and three owner
parameter qualifier differences in [TimedCommandQueue](../../../timed_command_queue.h).
These remain relevant when authoring the engine designs, not findings in the
current client view.

The parser accepted `const void*` but rejected the source spelling
`const void* const` at `RegisterImmediateEntry` before validation could run.
The validator subsequently treated the omitted top-level pointer `const` as
a mismatch. Dropping that qualifier from a by-value parameter does not change
the C++ function type; pointer-to-const was retained. Do not remove const from
the implementation to accommodate this notation limitation.

## Follow-Up

Keep readable source spellings and development-mode findings visible during
this pilot. Recheck on tooling upgrades before moving the unit to release
maturity. Separately reviewed tooling work should preserve qualified type and
alias identity across extraction and comparison, support pointer qualifiers,
and ignore only function-type-irrelevant top-level const on by-value parameters.
It must not erase pointee const or meaningful allocator distinctions.

The [unit rationale](../../../dependability/software_unit_design/client_connection.rst)
records cleanup behavior and regression coverage. Runtime tests address those
changes separately; they do not resolve these type-comparison findings. Native
QNX extraction remains a [separate tooling limitation](tooling-qnx-unit-analysis.md).