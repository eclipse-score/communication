# Bazel Traceability Tools

## Gtest As Exec

Based on //platform/bazel/rules:run_as_exec.bzl

### Usage in BUILD file

```python
load("@lobster//:lobster.bzl", "lobster_test", "lobster_gtest")
load("@//platform/aas/tools/traceability/bazel:gtest_as_exec.bzl", "test_as_exec")

test_as_exec(
    name = "tests_as_exec",
    executable = "@//platform/aas/mw/com:unit_test",
)

lobster_gtest(
    name = "gtest_lobster",
    testonly = True,
    tests = [":tests_as_exec"],
)
```

## Safety Analysis Target

```python
load("@//platform/aas/tools/traceability/bazel:safety_analysis.bzl", "safety_analysis")

safety_analysis(
    name = "sample_safety_analysis",
    failuremodes = [":sample_fmea_failure_modes"],
    fta = [":sample_fta"],
    controlmeasures = [":sample_fmea_control_measures"],
)
```

The Safety Analysis is based partly on TRLC and PlantUML, the arguments are specified as following:

- failuremodes: trlc_requirements
- fta: plantuml file
- controlmeasures: trlc_requirements

### Failuremodes
A failuremode contains

```
Abstract Type: Requirement
├── description: String
├── version: Integer
├── safety: Asil
├── note: optional String
├── status: Status
└── freeze_status: Status = Status.valid

Type: FailureMode extends ScoreReq.Requirement
├── guideword: GuideWord
├── failureeffect: String
└── rationale: optional String
```

### FTA

The Fta needs to be specified in the following format:

```
@startuml

!include fta_metamodel.puml

$TopEvent("Description", "ID")

$AndGate("ID", "Link to ID")

$IntermediateEvent("Description", "ID", "Link to ID")

$BasicEvent("Description", "ID", "Link to ID")

@enduml
```

The PlantUML Diagramm is processed as follows:
- Every Failuremode is specified in the ID of one TopEvent
- For every Basic Event a TRLC Requirement ID with the ID of the Basic Event is created

### Control Measures
The Control Measure is defined as a requirement:

```
Abstract Type: Requirement
├── description: String
├── version: Integer
├── safety: Asil
├── note: optional String
├── status: Status
└── freeze_status: Status = Status.valid

Type: FailureMode extends ScoreReq.Requirement
└── mitigates: String
```

Via lobster any Failuremode is Traces to a ControlMeasure
