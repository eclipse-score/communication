# Tooling for Traceability Concept

In this document the tooling for the platform traceability concept is described. It basically consists of 4 core Items:

1. Requirement Definition (TRLC)
1. Linking Requirements to Source (source_code_linker)
1. Linking Requirements to Tests (gtest, python)
1. Defining the Relations in Bazel
1. Rendering Traceability

An and additional item for the visualization of the requirements in Sphinx Needs:

1. TRLC Renderer

## Folder Structure

The Traceability Tooling is divided into configuration, tools and bazel rules.

The Tools TRLC and Lobster - which are also required by the traceability tooling - are python packages and are therefore configured
via third_party

### Definition of Requirements

For the definition of requirements a trlc file needs to be generated. The basic concept is described here:

https://github.com/score-software-engineering/trlc/blob/main/documentation/TUTORIAL-BASIC.md

Additionally a Plugin for VScode is available:

https://github.com/score-software-engineering/trlc-vscode-extension/releases

An example for defining trlc requirements targets:

```python
    load("@trlc//:trlc.bzl", "trlc_requirements", "trlc_requirements_test")

    trlc_requirements(
        name = "component_requirements",
        srcs = [
            "component_requirements.trlc",
        ],
        spec = [
            "//third_party/traceability/config/trlc/score_model:requirement_metamodel",
        ],
        visibility = ["//visibility:public"],
    )

    trlc_requirements_test(
        name = "component_requirements_test",
        reqs = [
            ":component_requirements",
        ],
        visibility = ["//visibility:public"],
    )

```

## Linking requirements to Source Code

For linking requirements to source code only a comment needs to be added to the cpp file:

```cpp
// trace: <trlc id>
```
Then it will be automatically available in the lobster traceability report

## Linking requirements to Test cases

#### CPP
Google test cases can be linked via an additional *record_property*:

```cpp
RecordProperty("lobster-tracing", "<TRLC Id>");
```

## Defining the bazel build targets

```python
load("//platform/aas/tools/traceability:traceability.bzl", "safety_software_unit", "safety_software_seooc")

safety_software_unit(
    name = " ... ",
    design = [" <PlantUML Detailed Design> "],
    impl = [" <Build Targets> "],
    reqs = [
        " <Component Requirement Bazel Target> ",
    ],
    tests = [
        " <Unit Test Cases> ",
        " <Component Integration Test Cases> ",
    ],
)

safety_software_seooc(
    name = " ... ",
    design = [" <PlantUML Documents> "],
    units = [" <Build Targets> "],
    reqs = [
        " <Assumed System Requirements> ",
    ],
    tests = [
        " <Integration Tests> ",
    ],
)

```
## Rendering Traceability

After linking all requirements to the defined levels and to code and implementation a traceability report can be generated via bazel test, e.g:

```bash
bazel build //third_party/traceability/doc/sample_library:safety_software_unit_example
```
