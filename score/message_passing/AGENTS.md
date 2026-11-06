# Message-Passing Agent Guidance

Scope: this directory and its descendants. This is developer guidance, not a
replacement for reviewed requirements, API contracts, or the safety argument.

## Essential Constraints

- Use C++17 and the [repository conventions](../../CONTRIBUTING.md).
- Run Bazel commands from the communication repository root, not this directory.
- `ClientConnection` supports QNX. Wrapper constraints do not determine runtime
  support. Track runtime support, tooling readiness, and analysis coverage
  separately; see [QNX tooling readiness](docs/knowledge/issues/tooling-qnx-unit-analysis.md).
- QNX dispatch is the safety-relevant backend; Linux Unix Domain is QM. Ground
  ASIL-B claims in actual QNX paths and relevant tests, not analogous Linux code.
- Keep intended contracts, observed behavior, reviewed decisions, and unresolved
  findings distinct. If sources disagree, expose the gap instead of silently
  rewriting a contract or inventing a mechanism to make the artefacts agree.
- Safety artefacts are unfinished. Do not treat placeholders or development-mode
  success as acceptance, or undertake a safety rewrite as an incidental fix.

## Read Only What the Task Needs

Read the matching topic before editing that area. Follow only the relevant
additional source, decision, or issue links for cross-cutting work. Do not load
every topic, skill, or record for an ordinary component task.

| Task | Read |
| --- | --- |
| C++ behavior, API, lifecycle, callbacks, memory | [Functionality](docs/knowledge/functionality.md) |
| Build, runtime tests, test tracing, validation commands | [Build and test](docs/knowledge/build-and-test.md) |
| Schema, parser, generated outputs, QNX analysis | [Tooling](docs/knowledge/tooling.md) |
| Requirements, ASIL, version pins, allocation | [Requirements](docs/knowledge/requirements.md) |
| PlantUML, public/internal API, component/unit wiring | [Architecture](docs/knowledge/architecture.md) |
| FMEA, FTA, root-cause assessments, own AoUs | [Safety analysis](docs/knowledge/safety-analysis.md) |
| Adding knowledge or configuring its discovery | [Maintenance guide](docs/knowledge/README.md) |

The topic pages link to existing repository skills. Use the applicable skill,
not a copied tutorial or a remembered workflow that is absent from the checkout.
Resolve tooling-sensitive questions against the pinned `@score_tooling` schema,
rules, and validator specifications; older notes and skill examples can lag it.

## Changes and Verification

- Start from the owning code path and its contract/test. Read implementation
  before claiming a failure mechanism exists; a diagram label is not evidence.
- Agree unclear requirement intent, architecture boundaries, and safety judgments
  with the relevant reviewer before changing formal artefacts.
- Start with a focused check for the touched behavior and follow the repository's
  contribution gates. The build/test page distinguishes runtime tests, TRLC
  verification, architectural consistency, and explicit manual SEooC checks.
- Recursive tests skip manual-tagged targets. Development maturity can permit
  findings without build failure. Inspect fresh logs and report skipped or
  failing checks accurately; never equate rendering with semantic validation.
- Keep tooling changes separate from component work. Do not patch an external
  tooling cache or weaken a contract just to clear a checker limitation.

## Maintain Shared Knowledge

Update owning knowledge with behavior, contract, or tooling changes. Topic pages
need edits only when their guidance or routing changes. Put independent decisions
and unresolved findings in separate descriptive records using the maintenance
guide. Preserve evidence and version-sensitive freshness; mark recovered intent
as provisional when approval evidence is unavailable.
Do not add private-memory dependencies, machine-specific paths, chronological
session logs, or competing instruction copies. Keep this entry point small;
routine findings belong in their owning topic/record, not another global rule.