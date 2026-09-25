# SCORE Message Passing Dependability Documentation Conventions

## Directory Structure

```
score/message_passing/dependability/
├── BUILD  # Main component and dependable_element targets
├── requirements/
│   ├── BUILD  # feature_requirements, component_requirements, trlc_requirements_ai_test
│   ├── feature_requirements.trlc  # Integration-level feature reqs (derived from assumed system)
│   └── component_requirements.trlc  # System, behaviour, API reqs (derived from features)
├── software_architectural_design/
│   ├── BUILD  # architectural_design target
│   ├── public_api.puml  # PlantUML public interface diagram
│   ├── private_api.puml
│   ├── static_design.puml
│   ├── client_connection_activity_diagram.puml
│   ├── server_client_sequence.puml
│   └── client-server.md  # Detailed design markdown (challenges, design decisions, examples)
├── assumed_system/
│   ├── BUILD  # assumed_system_requirements, assumptions_of_use (aous), trlc_requirements
│   ├── aous.trlc  # Assumptions of Use as TRLC
│   └── assumed_system_requirements.trlc  # System-level assumptions
├── safety_analysis/
│   ├── BUILD  # fmea, dependability_analysis targets
│   ├── failure_modes.trlc  # ISO 26262 failure modes with guidewords
│   ├── control_measures.trlc  # Mitigation strategies (mapped to FTA diagrams)
│   └── fta_*.puml  # Fault Tree Analysis diagrams (8 files)
└── software_unit_design/
    └── BUILD  # unit_design targets (placeholder, content in code)
```

## TRLC Patterns & Naming Conventions

### Package Structure
```
package MessagePassing
import ScoreReq
```
- Package names match module name
- Import ScoreReq for type definitions (FeatReq, CompReq, AssumedSystemReq, FailureMode, ControlMeasure, Mitigation, etc.)

### Feature Requirements (feature_requirements.trlc)
- **Type**: `ScoreReq.FeatReq`
- **Fields**: description, safety (ASIL.B/QM), derived_from, version
- **Naming**: PascalCase identifier (e.g., `ServerInterface`, `SafetyCertifiedTransportMechanism`)
- **Traceability**: `derived_from = [PackageName.ReqId@version]` (references assumed system requirements)
- **Safety**: Most are ASIL.B, some QM

Example:
```
ScoreReq.FeatReq ServerInterface {
    description = "..."
    safety = ScoreReq.Asil.B
    derived_from = [MessagePassing.SystemMessagingProtocol@1]
    version = 1
}
```

### Component Requirements (component_requirements.trlc)
- **Type**: `ScoreReq.CompReq`
- **Organized by sections**: "System Requirements", "Behaviour Requirements", "API Requirements"
- **Derived from**: Feature requirements (derivation links trace to features)
- **Naming**: Compound names reflect subsystem + characteristic
- **Fields**: description, safety, derived_from, version

Example:
```
ScoreReq.CompReq SafetyCertifiedTransportMechanismUnderQNX {
    description = "On the QNX operating system, ..."
    safety = ScoreReq.Asil.B
    derived_from = [MessagePassing.SafetyCertifiedTransportMechanism@1, ...]
    version = 1
}
```

### Assumed System Requirements (assumed_system_requirements.trlc)
- **Type**: `ScoreReq.AssumedSystemReq`, `ScoreReq.Mitigation`
- **Top-level assumptions**: System shall provide messaging, safe state = "safe-silent"
- **Rationale field**: Explains why assumption is necessary

### Failure Modes (failure_modes.trlc)
- **Type**: `ScoreReq.FailureMode`
- **Fields**: guidewords, description, failureeffect, interface, version, safety
- **Guidewords**: ISO 26262 HARA guidewords (LossOfFunction, PartialFunction, Corrupted, UnintendedFunction, TooEarly, TooLate)
- **Interface**: Lists affected APIs
- **Categories by concern**: Channel availability, message delivery, notifications, server handling, lifecycle, context data

Example:
```
ScoreReq.FailureMode MessageNotDeliveredCorrectly {
    guidewords = [ScoreReq.Guideword.LossOfFunction, ...]
    description = "A message... is not... delivered."
    failureeffect = "The receiver processes no data..."
    interface = "IServerConnection.Reply, IClientConnection.Send, ..."
    version = 1
    safety = ScoreReq.Asil.B
}
```

### Control Measures (control_measures.trlc)
- **Type**: `ScoreReq.ControlMeasure`
- **Fields**: description, safety, mitigates, version
- **Mitigates**: Links to failure mode name (must match $BasicEvent in FTA diagrams)
- **Purpose**: Traces how design mitigates failure modes

Example:
```
ScoreReq.ControlMeasure OsIpcFaultHandling {
    description = "The component shall detect OS IPC failures by checking..."
    safety = ScoreReq.Asil.B
    mitigates = "MessagePassing.MessageNotDeliveredCorrectly"
    version = 1
}
```

## Bazel/Build Conventions

### Requirements BUILD File
```starlark
load("@score_tooling//bazel/rules/rules_score:rules_score.bzl",
    "component_requirements",
    "feature_requirements",
)
load("@score_tooling//validation/ai_checker:ai_checker.bzl",
    "trlc_requirements_ai_test",
)

feature_requirements(
    name = "feature_requirements",
    srcs = ["feature_requirements.trlc"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//score/message_passing/dependability:__subpackages__"],
    deps = ["//score/message_passing/dependability/assumed_system:assumed_system_requirements"],
)

trlc_requirements_ai_test(
    name = "feature_requirements_ai_check",
    batch_size = 10,
    reqs = [":feature_requirements"],
    tags = ["manual"],
)

component_requirements(
    name = "component_requirements",
    srcs = ["component_requirements.trlc"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//score/message_passing/dependability:__subpackages__"],
    deps = [
        ":feature_requirements",
        "//score/message_passing/dependability/assumed_system:assumed_system_requirements",
    ],
)
```

### Architectural Design BUILD File
```starlark
load("@score_tooling//bazel/rules/rules_score:rules_score.bzl",
    "architectural_design",
)

architectural_design(
    name = "message_passing_architectural_design",
    public_api = ["public_api.puml"],
    static = ["static_design.puml", "client-server.md", "private_api.puml"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//score/message_passing/dependability:__subpackages__"],
)
```

### Safety Analysis BUILD File
```starlark
load("@score_tooling//bazel/rules/rules_score:rules_score.bzl",
    "dependability_analysis",
    "fmea",
)

fmea(
    name = "message_passing_fmea",
    arch_design = "//...:message_passing_architectural_design",
    controlmeasures = ["control_measures.trlc"],
    failuremodes = ["failure_modes.trlc"],
    root_causes = [":fta_files"],
)

dependability_analysis(
    name = "message_passing_dependability_analysis",
    arch_design = "//...:message_passing_architectural_design",
    fmea = ["//...:message_passing_fmea"],
    tags = ["manual"],
)
```

### Top-level Component BUILD File
```starlark
dependable_element(
    name = "dependable_element_message_passing",
    architectural_design = ["//path:architectural_design"],
    assumptions_of_use = ["//path:aous"],
    components = [":component_message_passing"],
    dependability_analysis = ["//path:dependability_analysis"],
    integrity_level = "B",  # ASIL B
    maturity = "development",
    requirements = [
        "//path:feature_requirements",
        "//path:assumed_system_requirements",
    ],
    tests = ["//path:unit_tests"],
    deps = ["//third_party/deps"],
)
```

### Unit Design BUILD File
```starlark
load("@score_tooling//bazel/rules/rules_score:rules_score.bzl", "unit_design")

unit_design(
    name = "client_connection_unit_design",
    visibility = ["//score/message_passing:__pkg__"],
)
```

## Documentation Markdown Patterns

### Architectural Design Markdown (client-server.md)
- **Sections**: Challenges, General approach, Client/Server subsections, Implementation considerations, Safety concerns, Examples
- **Subsections in design sections**:
  - Connection state machines (Stopped, Starting, Ready, Stopping states)
  - Callback mechanisms and threading model
  - Message sending/delivery guarantees
  - Resource management (factory pattern)
- **Safety sections**: Safe/QM client/server scenarios, timing guarantees
- **Examples**: Real use cases (DataRouter logging, subscribers) with protocol descriptions

### PlantUML Diagrams
- **public_api.puml**: High-level class interfaces (IServer, IClientConnection, etc.)
- **private_api.puml**: Internal implementation details
- **static_design.puml**: Static class relationships
- **client_connection_activity_diagram.puml**: Activity flow
- **server_client_sequence.puml**: Message sequence charts

## Sphinx/Documentation Integration

### RST Documentation Structure
- `docs/sphinx/message_passing.rst`: Overview + generated API docs via Breathe
- `docs/sphinx/how_to_document.rst`: Guidelines for Doxygen @api tag + Breathe integration
- **@api Tag Convention**: C++ classes/functions tagged with @api are auto-documented
- **Doxygen Integration**: XML → RST generation for namespace/class/member reference

## Traceability & Linking

- Feature Reqs → Assumed System Reqs (derived_from links)
- Component Reqs → Feature Reqs (derived_from links)
- Failure Modes → interfaces + components
- Control Measures → Failure Modes (mitigates field)
- FTA Diagrams → Control Measures (via $BasicEvent aliases)
- AI Checker: Validates TRLC syntax + semantic completeness

## Key Patterns to Reuse

1. **TRLC Derivation Chain**: Assumed System → Features → Component Requirements
2. **Safety Nomenclature**: ASIL.B (Automotive Safety Integrity Level B)
3. **Section Organization**: Group component reqs by concern (System, Behaviour, API)
4. **API-centric Failure Modes**: Each failure mode lists affected interfaces
5. **Markdown + PlantUML Blend**: Prose design decisions + visual architecture
6. **Bounded Visibility Scoping**: `visibility = ["//path:__subpackages__"]`
7. **Dependency Management**: Requirements reference upstream requirements

## FTA metamodel fact (confirmed 2026-09-17)

`$BasicEvent` aliases in `fta_*.puml` resolve to EITHER a `ScoreReq.ControlMeasure` (in
`control_measures.trlc`) OR a `ScoreReq.AoU` (in `assumed_system/aous.trlc`) — both extend a common
`Measure` base, so an AoU-backed basic event is fully valid, not a special case. Use `AoU` for root
causes only the integrator/caller can close (contract misuse, timing supervision left to the
integrating system), `ControlMeasure` for causes the component itself detects/handles at runtime.
Source: `.github/skills/score-safety-analysis/SKILL.md` Step 3 table + "Common Mistakes" row
("AoU added to control_measures.trlc" is listed as a mistake — AoUs belong in `aous.trlc`).

The vendored `third_party/score_requirement_model/score_requirements_model.rsl` copy in this repo
is stale/incomplete (confirmed empty of `Guideword`/`Mitigation`/`FailureMode` defs on 2026-09-24) —
the real, current schema comes from the `@score_tooling` external Bazel repo at
`bazel/rules/rules_score/trlc/config/score_requirements_model.rsl` (find it via e.g.
`find ~/.cache/bazel -iname score_requirements_model.rsl -path '*score_tooling*'` after a build).
Confirmed there: `Guideword` enum has 9 values (not just the ones currently used in this repo's
`failure_modes.trlc`) — `LossOfFunction, PartialFunction, Corrupted, UnintendedFunction, TooEarly,
TooLate, Wrong, DelayedFunction ("processing too slow", distinct from TooLate), ExceedingFunction,
ArbitraryExecution`. `AoU extends ControlMeasure` (not a plain sibling); `Mitigation extends
AssumedSystemReq` (a different type again, used for a different traceability direction). The tuple
`Measure { item [ControlMeasure, PreventiveMeasure, Mitigation] }` is satisfied by `AoU` too since
it IS-A `ControlMeasure`.

Also: this repo's `architectural_design` Bazel rule runs diagrams through a custom Rust `puml_cli`
parser (score_tooling), which is stricter/narrower than real PlantUML — confirmed incompatible with:
`skinparam <Type> { key value }` block syntax (must use single-line `skinparam TypeKey value`
instead), PlantUML state-diagrams (`state X`, `X --> Y` — not recognized under any of its
Component/Activity/Class/Sequence parsers), and sequence-diagram lost-message arrows (`-x`, use
plain `->` instead) and the `...` narrative-continuation line (use a `note` instead). Always
`bazel build //<path>:<architectural_design target>` after editing/adding `.puml` files to catch
this before considering the work done.

## Two-tier FTA methodology (added 2026-09-17, opt-in skill)

Human clarified a methodology point not previously written anywhere: distinguish an *informal,
never-TRLC-wired* "virtual" system-level FTA (root nodes = negated `AssumedSystemReq`/`Mitigation`
records, pure team-understanding scratchpad) from the *actual* `fta_<failure_mode>.puml` files,
which are correctly understood as **micro-FTAs** scoped to a component's public-API methods
(`FailureMode.interface`) — not system-level trees. Key relationship: a micro-FTA's leaf/basic
events (root causes) should generally be a subset of a hypothetical Tier-1 tree's leaves; internal
nodes need not correspond; requirement layering (system→feature→component) is a loose causal-layer
guide, not a strict containment rule. Being mechanically well-scoped (interface-keyed, aliases
resolve) does NOT mean a `FailureMode`/`fta_*.puml`'s content is semantically valid, unambiguous,
correctly attributed, or complete — that needs an explicit review pass, separate from wiring.
User explicitly did NOT want `score-safety-analysis/SKILL.md` itself edited (not everyone should be
forced to adopt this) — instead created a new, separate, opt-in skill
`.github/skills/score-safety-analysis-tiered-fta/SKILL.md` layering this on top. Use that skill
(not score-safety-analysis alone) whenever asked to review/redo existing FTA content, not just
wire new records.

## Meta-process artifacts (added 2026-08-22)

- Repo has 4 mechanical skills at `.github/skills/{score-requirements,score-architecture,score-safety-analysis,score-testing}/SKILL.md`. They all dangling-reference an orchestrator skill named `rules-score` in their "Not for" sections — this was missing, so it was created at `.github/skills/rules-score/SKILL.md`. It defines the top-down lifecycle (problem statement → assumed system/AoU → feature reqs → architecture design → component reqs → Bazel wiring → safety analysis → impl delta → tests → validation), the freeze-after-checkpoint discipline, and the generic `<component>/research/` scratchpad directory convention (problem_statement.md, inputs/, work_log.md, next_steps.md, references.md, backlog.md, nice_to_haves.md).
- Component-specific kickoff prompts live at `.github/prompts/*.prompt.md` in this repo (git-shared, not user-profile prompts). Example: `bootstrap-service-discovery-seooc.prompt.md` bootstraps `score/service_discovery/research/` (new SEooC, NOT under mw/com) from the PoC at `score/mw/com/service_discovery/`, integrator `score/mw/com/`, and dependency `score/message_passing/`.
- 2026-08-24: Ran that bootstrap. `score/service_discovery/research/` now exists with problem_statement.md + inputs/work_log/next_steps/references/backlog/nice_to_haves, awaiting Checkpoint 0 human sign-off (Step 0 of rules-score). Key evidence found: the actual flock/inotify marker-file mechanism (`ServiceExistanceMarkerFile`/`ServiceUsageMarkerFile`) lives in `score/mw/com/impl/bindings/lola/` (skeleton.h/proxy.h/partial_restart_path_builder.cpp), NOT in the service_discovery/ PoC — the PoC only has in-memory creation/usage locks. `message_passing`'s ServiceProtocolConfig fixes max_send/reply/notify_size per protocol kind at setup time with no transport-level chunking (EMSGSIZE on overflow) — any SD batching design needs its own app-level splitting strategy. Do not resume Step 1 (assumed-system requirements) until a human confirms the problem statement's open questions.
- When asked to rework a PoC dependable_element into a proper process-compliant one, use `rules-score` skill as the entry point rather than re-deriving the workflow ad hoc.
- 2026-09-16: Wrote `score/message_passing/dependability/software_architectural_design/memory_and_size_limits_findings.md` — a findings-only doc (no TRLC yet) analyzing missing count/length/memory specs. Key facts for future work: `ServiceProtocolConfig::identifier` hard limits are backend-specific and undocumented (Unix Domain abstract socket ≈106 bytes, silently truncated via memcpy in `unix_domain_socket_address.h`; QNX `QnxResourcePath::kMaxIdentifierLen`=256, hard precondition panic on violation) — no cross-backend validation exists. Preferred identifier size ≤15 bytes to fit libstdc++ `score::cpp::pmr::string` SSO (avoids heap alloc/fragmentation); libc++ SSO is 22 bytes. `ServerConfig::pre_alloc_connections` and server-side `max_queued_sends` are dead/unused in BOTH `UnixDomainServer` (ctor param unnamed) and `QnxDispatchServer` (stored but unread) — contradicts existing ASIL B reqs `ServerPreallocatesConnectionObjects`/`ServerRingBufferQueueSizeConfigurable` in component_requirements.trlc; ServerConnection objects are always heap-allocated lazily at accept time, never preallocated at construction. Both engines (`unix_domain_engine.cpp`/`qnx_dispatch_engine.cpp`) share ONE monotonically-growing `posix_receive_buffer_` per engine (sized to max `max_receive_size` seen), not per-connection — good property, currently undocumented. No test proves memory_resource containment (i.e. that code never falls back to `get_default_resource()`); doc sketches a poisoned-default-resource + counting-resource test technique as future work, deliberately not implemented per user request (user wanted findings documented, not tests/TRLC written yet).
- 2026-09-23: Renamed skill `.github/skills/rules-score-actualize/` → `.github/skills/rules-score-update/` (all cross-references across `.github/skills/*`, `.github/prompts/*`, and `score/message_passing/research/**` updated to `rules-score-update`). Also: `assumed_system_requirements.trlc`'s writing-convention comment block (which referenced a specific `research/changes/` cycle) was moved out into a new `score/message_passing/dependability/README.md` (general "keep dependability/ files free of process/meta-info" convention + the AssumedSystemReq black-box-capability writing convention specifically), leaving only a one-line pointer in the `.trlc` file. Use this README.md pattern for other `dependability/` dirs that need similar meta-info moved out.
## puml_cli sequence-diagram resolver bug (found 2026-09-24)

Confirmed via direct bisection with the standalone `puml_cli` binary (`bazel run
@score_tooling//plantuml/parser/puml_cli`, `--diagram-type sequence`): a sequence `.puml` file
that contains **any `alt`/`loop` group whose branch contains a nested single-line `note over X :
text` AND, anywhere else in the same file (before or after, adjacent or not), a separate
multi-line `note over X\n...\nend note` block** — the resolver (not the parser; parsing itself
succeeds, logged as "Successfully parsed") reports a false `unterminated sequence group`, pointing
at an unrelated, already-correctly-closed `alt`/`end` pair. Minimal repro (7 lines) confirmed with
a fully synthetic file, so it is not content-specific to this repo. **Workaround: never use a
multi-line `note X\n...\nend note` block in any sequence diagram that also has a nested
single-line note inside an `alt`/`loop` branch — convert ALL multi-line notes to the single-line
`note over/left/right X : line1\nline2\n...` colon form instead** (embed line breaks as literal
`\n` inside the one physical line). This is now the repo convention for all sequence `.puml`
files, not just a one-off fix.

Separately (real PlantUML/Sphinx rendering, not the custom validator): a `create X` statement's
first message must have `X` as the **target** (arrow head), e.g. `A -> X : ...`, not `X -> A :
...` — otherwise real PlantUML (used only for the Sphinx HTML doc build, a different pipeline than
`puml_cli`) emits `After create command, you have to send a message to "X"` and fails to render
that diagram's HTML (bazel build itself still succeeds; only `dependable_element`-level doc
targets surface it as a WARN in the build log).

Also confirmed (documented behavior gap): `sequence_internal_api`'s Method-Name Consistency does
**not** actually exempt calls touching the special `ExternalEndpoint` participant, contrary to a
literal reading of `component_sequence.md`'s exemption (that exemption is for a *different*
validator/check — Interface-Connection Consistency). Any solid **or** dashed arrow to/from
`ExternalEndpoint` with a real method name in parens (e.g. `ExternalEndpoint -> unit : Create(...)`)
still gets checked against the internal API diagram and fails if that name isn't declared there —
even though public-API method names belong in `public_api.puml`, which this validator never reads.
Confirmed workaround (proven, not just theorized): wrap the whole label in an outer pair of parens,
e.g. `(Create(protocol_config, server_config))` — method-name extraction takes "text before the
first `(`", which is then empty and is *silently skipped* per the sequence_internal_api spec's own
stated rule for empty method names. Also confirmed: `static_design.puml`'s "Component" grammar does
**not** support method bodies on interface declarations (`interface X { +Foo() }` inside a
component diagram is a hard parse error, "expected [empty_line, diagram_statement]") — only
`public_api.puml`/`private_api.puml` (Class grammar) support that; never try to add methods to a
top-level/nested interface in the component (`static`) diagram itself.

## Message Passing architecture correction (2026-09-24, from direct human code-reading)

`ISharedResourceEngine` exists **only** to make `ClientConnection` platform-independent (one
concrete `ClientConnection` class shared by both backends, injected with either
`UnixDomainEngine` or `QnxDispatchEngine` through this one interface). The **server-side**
implementations (`UnixDomainServer`/`QnxDispatchServer` and their nested `ServerConnection`
classes) are **not** unified behind `ISharedResourceEngine` at all — each backend's server-side
code is bespoke per platform and uses its own **concrete** engine type directly, with no shared
abstraction. Do not model `server_connection` as sharing `ISharedResourceEngine` with
`qnx_dispatch`/`unix_domain` in any future architecture diagram — bind it only from
`client_connection`. Since `server_connection` (a `<<unit>>` with `implementation =
[":common_headers"]`, i.e. interface-only, no concrete class of its own) then has no internal
(non-`ExternalEndpoint`) interface connection to any other unit at all, sequence diagrams must
route all of its interactions either to/from `ExternalEndpoint` (exempt from interface-connection
checks) or describe qnx_dispatch/unix_domain's internal handling via `note over` — never a direct
arrow crossing to `server_connection` from those units.

Also per the same human review: **QNX dispatch is the safety-relevant (ASIL B) backend and the
primary reason the `os` (required, environment) interface exists at all**; Unix Domain (QM,
Linux-only) is secondary and uses a smaller subset of the same `score::os` wrappers. The real,
concrete OS abstraction layer is the `score::os::*` classes (confirmed in
`qnx_dispatch_engine.h`'s `OsResources` struct: `Channel`, `Dispatch`, `Fcntl`, `IoFunc`, `Signal`,
`qnx::Timer`, `SysUio`, `Unistd`) — ground the `os` interface in `public_api.puml` in these real
class names, not in `ISharedResourceEngine`'s method names (a different, internal abstraction
layer). Thread/synchronization primitives are arguably also part of the OS abstraction but are
deliberately excluded for brevity (human's explicit scope call, not yet revisited).

- 2026-08-31: Added a companion skill `.github/skills/rules-score-update/SKILL.md` (originally named `rules-score-actualize`, renamed 2026-09-23) for the case `rules-score`'s "Scope & limits" flagged as unwritten follow-up: incrementally updating an EXISTING/trusted (not discardable-PoC) dependable_element baseline — new need, defect, drift, or deprecation. Core idea: impact analysis (upward root-cause trace + downward ripple trace via derived_from/FTA aliases/lobster-tracing) BEFORE any edit, then minimal-diff cascade with version-bump+re-pin discipline; freeze anything outside the traced impact set. Its `research/` convention differs from rules-score's one-shot layout: `research/changes/<date>-<slug>/` holds one directory per actualization cycle (change_request.md, impact_analysis.md, work_log.md, next_steps.md, evidence_bundle.md), with `problem_statement.md`/`references.md`/`backlog.md`/`nice_to_haves.md` shared long-lived across all cycles at the top of `research/`. Updated rules-score's "Scope & limits"/"Not for" to point at it instead of leaving it as a TODO. Example prompt: `.github/prompts/actualize-message-passing-seooc.prompt.md` bootstraps `score/message_passing/research/` (dependable_element already exists: integrity_level=B, maturity=development) with a reverse-documented baseline snapshot (NOT a fresh Step-0 derivation), notes `score/mw/com/impl/bindings/lola/messaging/` as one known non-exclusive consumer for future impact-analysis orientation, then stops to ask the human for the first concrete change trigger (no change_request.md is written yet, no concrete change was specified when this prompt was authored).
