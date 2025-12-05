# Traceability Concept

## General Metamodel

Current detailed definition of the Metamodel:

![Traceability Metamodel](https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/swh/safe-posix-platform/third_party/traceability/doc/assets/metamodel.drawio.svg)

Simplified Metamodel for Requirements and Architecture Definition

![Simplified Traceability Metamodel](https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/swh/safe-posix-platform/third_party/traceability/doc/assets/metamodel_simple.drawio.svg)

## Requirements Metamodel

The Requirements Definition is done in TRLC, therefore folowing metamodel (.rsl file) was defined:

```
Requirement (abstract)
│
├── AssumedSystemReq (extends Requirement)
│   └── AssumedSystemReqId (tuple)
│
├── FeatReq (extends Requirement)
│   └── FeatReqId (tuple)
│       └── derived_from: AssumedSystemReqId[1..*]
│
└── CompReq (extends Requirement)
    └── derived_from: FeatReqId[1..*]
```

It consists of 4 Types:
- Abstract Requirements Type
- Assumed System Requirement
- Feature Requirement
- Component Requirement

A more detailes view including all attributes is displayed here:

```
Requirement (abstract)
│   ├── description: String
│   ├── version: Integer
│   ├── safety: Asil
│   ├── note (optional): String
│   ├── status: Status
│   └── freeze status = Status.valid
│
├── AssumedSystemReq (extends Requirement)
│   ├── rationale: String
│   └── AssumedSystemReqId (tuple)
│       ├── item: AssumedSystemReq
│       ├── separator: @
│       └── LinkVersion: Integer
│
├── FeatReq (extends Requirement)
│   ├── derived_from: AssumedSystemReqId[1 .. *]
│   └── FeatReqId (tuple)
│       ├── item: FeatReq
│       ├── separator: @
│       └── LinkVersion: Integer
│
└── CompReq (extends Requirement)
    ├── derived_from: FeatReqId[1 .. *]
```
