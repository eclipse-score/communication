# S-CORE Communication CI Build Jobs and Pipelines Design

This document describes the design of all jobs running in the S-CORE Communication repository.
In places where the current implementation does not match the design, a ticket to change the implementation is attached.
There shall be no job running in the CI that is not stated in this document.

For each job running in the CI, it shall be possible for a developer to run it locally. It shall be possible to run it
fully or partially by providing specific targets. This ensures efficient use of resources because developers have an easy
way to test jobs in advance.
When a job fails, apart from the relevant logs, it shall clearly state how to reproduce it locally. All jobs must follow
the same mechanism to communicate that, a developer should not get that information in different ways depending on the job.
In all jobs, the command to reproduce should show the fastest way of reproduction. For example, if a target fails to build,
the command line should be such that only builds that target.

## Modifying this standing document

Any change to this document needs to be proposed via pull request, and approved by the codeowners
[by consensus](https://en.wikipedia.org/wiki/Consensus_decision-making).

It is requested to add the motiviation of the change in the pull request description and explain what goals the change is trying to fulfill.

If the change request has not been discussed in advance with any of the codeowners, we encourage to present the change on [Monday's communication sync](https://github.com/orgs/eclipse-score/discussions/112).

## Project goals

Our CI infrastructure is there to support us on fulfilling the project goals. Each CI job should support achieving at least
one of them. Here we list the goals, and we add a shortcut to them to be able to refer to them below. The number or the order
could be random. For the purpose of this design we consider all goals equal.

- SCOM-PG-01: Deliver high quality SW
- SCOM-PG-02: Deliver stable SW
- SCOM-PG-03: Deliver functionally safe SW
- SCOM-PG-04: Use resources efficiently
- SCOM-PG-05: Satisfy all functional requirements
- SCOM-PG-06: Satisfy all none-functional requirements
- SCOM-PG-07: Satisfy our processes and enhancement proposals

## Supported configurations

S-CORE Communication supports multiple architectures, compilers, and operating systems. Some of them are only supported for development and
some are supported for production.
In the CI we do not test we do not test all combinations in all pipelines, we try to find a combination that helps us to find most of the issues
without duplication. If we believe that most issues are dected with certain combinations, the rest of combinations might not run in the check and
merge pipeline but only in the nightly pipeline.

Below is the list of supported configurations:

|              | GCC 12.2 + libstdc++ | GCC 15 + libstdc++   | QCC (GCC 12.2) + libc++ | Clang 22 + libc++    |
|--------------|----------------------|----------------------|-------------------------|----------------------|
| x86-64 linux | Only for development | Only for development | NA*                     | Only for development |
| ARM64 linux  | Only for development | Only for development | NA*                     | Only for development |
| x86-64 QNX   | NA*                  | NA*                  | Yes                     | NA*                  |
| ARM64 QNX    | NA*                  | NA*                  | Yes                     | NA*                  |

Note: Configuration marked as "NA" are considered as not applicable because the architecture/compiler/operating system configuration is in general not supported.

And this is the list of bazel `--config=` for the supported configurations:

|              | GCC 12.2 + libstdc++                          | GCC 15 + libstdc++ | QCC (GCC 12.2) + libc++ | Clang 22 + libc++        |
|--------------|-----------------------------------------------|--------------------|-------------------------|--------------------------|
| x86-64 linux | --config=linux_x86_64_score_gcc_12_2_0_posix  | (default)          | NA                      | Config not yet available |
| ARM64 linux  | --config=linux_aarch64_score_gcc_12_2_0_posix | Not supported yet  | NA                      | Config not yet available |
| x86-64 QNX   | NA                                            | NA                 | --config=qnx_x86_64     | NA                       |
| ARM64 QNX    | NA                                            | NA                 | --config=qnx_arm64      | NA                       |

## Pipelines

### Check and merge queue

Check and merge are designed to ensure certain quality of the SW before a change request can be merged to the main branch.
Any job in check and merge queue must be robust against flakiness. That is crucial for the developers to gain confidence in the
CI. At the same time, we do not want to have unstable software, however, this is not the task of the check and merge.
Preventing flakiness is achieved with the periodic pipelines.

Additionally, check and merge jobs must be relatively fast, otherwise the feedback cycle becomes too long. For that reason,
no job running on check and merge should take longer than 20 minutes (daily average).

#### Check that PRs to be merged have no merge commits

When creating merge commits, conflicts might arise. If that is the case, and conflicts need to be resolved, it might be
hard to see in a pull request what was change. Because of that, to reduce the chance that unintentional changes are merged,
we forbid merge commits in pull requests.

#### Check for exactly one new line at the end of the file

Time and focus is lost in a PR review when files do not have exactly one new line at the end of the file. To save time from everyone
we want this to be enforced in checks and not rely on the reviewers.

#### Markdown formatter and linter

Ensure that the Markdown files are formatted following our decided style.
The goal is not to ensure a specific format but to have consistency in the project.
With a linter, we also want to reduce the possibility of errors and maximize compatibility across different Markdown viewers.

#### Formatting and linting of Bazel files

Ensure that all Bazel files (`BUILD`, `*.bzl`, `WORKSPACE`, etc.) are formatted in the same way.
Also, it lints the files to ensure the basic Bazel best practices.

#### Clang format

Ensure that the modified C++ files are formatted following our decided style.
The goal is not to ensure a specific format but to have consistency in the project.

#### Build everything and run unit tests (x86-64 linux)

Builds everything and runs all unit tests for all languages (C++, Rust, Python, etc.).

A Bazel unit test target should not take more than 10 seconds to execute. Short time ensures several things:

- that these are really unit tests
- that we do not have big targets with too many unit tests on it
- that the total time for running all unit test is not longer than necessary
- that we have a fast job that developers can run locally to test API breaking changes in common libraries
- that when we run test targets multiple times to detect flakiness, the running times will not explode

To ensure that the job is fast, the targets should be built with the right combination of flags that provides a fast enough
compilation but still keeping a fast enough execution. To ensure that the job is robust, a failing test
will be retried up to 3 times.

#### Run C++ unit tests under address, UB, and leak sanitizers (x86-64 linux)

Runs all C++ unit tests under address, undefined behavior (UB), and leak sanitizers.

Each unit test target also has a maximum run time [as the job for unit tests without sanitizers](./CI.md#build-everything-and-run-unit-tests-host)
and the same stability design where a test is rerun 3 times in case of failure.

This job cannot be combined with any of the other sanitizer jobs due to how sanitizers work.
If the tool used for sanitizers is ever changed, this decision could be revisited.

#### Run C++ unit tests under thread sanitizer (x86-64 linux)

Runs all C++ unit tests under thread sanitizer.

Each unit test target also has a maximum run time [as the job for unit tests without sanitizers](./CI.md#build-everything-and-run-unit-tests-host)
and the same stability design where a test is rerun 3 times in case of failure.

In case that a test had to be retried, that information is uploaded to some storage and made it visible in Power BI. Such
information will only be collected from the gate pipeline.

This job cannot be combined with any of the other sanitizer jobs due to how sanitizers work.
If the tool used for sanitizers is ever changed, this decision could be revisited.

#### Run integration tests (x86-64 linux)

#### Run integration tests (ARM64 QNX)

#### Run all C++ tests with the hardned standard library (libstdc++) (x86-64 linux)

#### Run all C++ tests with the hardned standard library (libc++) (x86-64 linux)

#### Clang-tidy checks (x86-64 linux)

Runs the [clang-tidy checks](./.clang-tidy) for the host.

#### Clang-tidy checks (ARM64 QNX)

Runs the [clang-tidy checks](./.clang-tidy) for the target.

#### API checks

Make sure that the publicly visible targets are part of the public API specification.
Also makes sure that we do not break the API (source-code compatibility) accidentally.

#### Build score communication example

Makes sure that S-CORE Communication can be integrated as a module, and that it can work with different versions of Bazel and Bazel modules.

#### Safety sentinel checks

### Periodic 24h (at 20:00 UTC)

#### Job to check for deprecated warnings

#### CodeQL checks (x86-64 linux)

#### CodeQL checks (ARM64 QNX)

#### Code coverage (x86-64 linux)

#### Code coverage (x86-64 QNX)

#### Flaky detection by running unit tests

Ensures that our SW is stable by running each unit tests 20 times. If a test fails
at least one time, it is considered a bug and a ticket is created.

The setup of this job is exactly the same as [the one running in check and merge](#build-everything-and-run-unit-tests-x86-64-linux)
but with the addition of the flag `--runs_per_test=20`.

#### Flaky detection by running integration tests

Ensures that our SW is stable by running each unit tests 20 times. If a test fails
at least one time, it is considered a bug and a ticket is created.

The setup of this job is exactly the same as [the one running in check and merge](#run-integration-tests-x86-64-linux)
but with the addition of the flag `--runs_per_test=20`.

## Post-mortem analysis and follow-up actions

It is acknowledged that this design is not perfect, and experience will teach us that this design will have to be adapted.
Most changes to this document should be triggered by new requirements or because of a follow-up action from a post-mortem
analysis.

Also remember that the decision of moving a job from a late pipeline to an earlier pipeline should not be made based on
singularities and without considering testing strategies of affected components. For example, some post-mortem might conclude
that an issue could a) have been prevented by moving a job from "nightly" to "check and merge" or b) by creating a unit test. In such
case the latter option shall alway be the preferred one.
