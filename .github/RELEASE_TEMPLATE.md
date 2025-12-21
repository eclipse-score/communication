Module Name: communication
Release Tag: <release-tag>
Origin Release Tag: <previous-release-tag>
Release Commit Hash: <commit-sha>
Release Date: <release-date>

Overview
--------
The communication module provides a generic communication frontend with an IPC binding for use in the S-CORE project.

The module is available as a Bazel module in the S-CORE Bazel registry: https://github.com/eclipse-score/bazel_registry/tree/main/modules/score_communication

Disclaimer
----------
This release is not intended for production use, as it does not include a safety argumentation or a completed safety assessment.
The work products compiled in the safety package are created with care according to the [S-CORE process](https://eclipse-score.github.io/process_description/main/index.html). However, as a non-profit, open-source organization, the project cannot assume any liability for its content.

For details on the features, see https://eclipse-score.github.io/score/main/features/communication/index.html

Improvements
------------


Bug Fixes
---------

Compatibility
-------------
- `x86_64-unknown-linux-gnu` using [score_toolchains_gcc](https://github.com/eclipse-score/toolchains_gcc)
- `x86_64-unknown-linux-gnu` using [toolchains_llvm](https://github.com/bazel-contrib/toolchains_llvm)
- `x86_64-unknown-nto-qnx800` using [score_toolchains_qnx](https://github.com/eclipse-score/toolchains_qnx)
- `aarch64-unknown-nto-qnx800` using [score_toolchains_qnx](https://github.com/eclipse-score/toolchains_qnx)


Performed Verification
----------------------
- Unit test execution on host with all supported toolchains
- Build on supported target platforms (QNX8 x86_64 and QNX8 aarch64)
- Thread sanitized unit test execution
- Address and UB sanitized unit test execution
- Leak sanitized unit test execution

Known Issues
------------


Upgrade Instructions
--------------------
Backward compatibility with the previous release is not guaranteed.

Contact Information
-------------------
For any questions or support, please raise an issue/discussion.
