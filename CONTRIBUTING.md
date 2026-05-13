# Contributing to the Communication Module

- Thank you for your interest in contributing to the Communication Module of the Eclipse SCORE project!
- This guide will walk you through the necessary steps to get started with development, adhere to our standards, and successfully submit your contributions.

## Eclipse Contributor Agreement (ECA)

Before making any contributions, **you must sign the Eclipse Contributor Agreement (ECA)**. This is a mandatory requirement for all contributors to Eclipse projects.

Sign the ECA here: https://www.eclipse.org/legal/ECA.php

Pull requests from contributors who have not signed the ECA **will not be accepted**.

## Contribution Workflow

To ensure a smooth contribution process, please follow the steps below:

1. **Fork** the repository to your GitHub account.
2. **Create a feature branch** from your fork:
   ```bash
   git checkout -b feature/your-feature-name
   ```
3. **Write clean and modular code**, adhering to:
   - **C++17 standard**
   - **Google C++ Style Guide**
4. **Add tests** for any new functionality.
5. **Ensure all tests pass** before submitting:
   ```bash
   bazel test //...
   ```
6. **Open a Pull Request** from your feature branch to the `main` branch of the upstream repository.
   - Provide a clear title and description of your changes.
   - Reference any related issues.

## Development Setup

### General Prerequisites

- Bazel (Instructions for installing here: https://bazel.build/install)
- Docker (Instructions for installing here: https://docs.docker.com/engine/install/)
  - Note. Running Docker in rootless mode is not yet officially supported but may work. See https://docs.docker.com/engine/security/rootless/ for more information.

### OS-specific Prerequisites

We strive to be independent of the host platform via bazel sandboxing.
Some host platforms come with limitations that bazel cannot sandbox sufficiently.
For these platforms we collect instructions below.

Please be aware, that while we officially support Ubuntu 24.04 as the host platform that we also test in our CI.
While other platforms generally should work, we can give no such guarantee.
Should you face issues with your host platform, feel free to raise an issue or discussion where we will try to support.

#### Ubuntu 24.04 and newer

Starting with Ubuntu 24.04 the security framework apparmor was introduced.
The standard configuration of apparmor prohibits unprivileged user namespaces.
This interferes with the bazel sandboxing mechanism and inhibits the linux-sandbox.
Bazel falls back to a less powerful sandboxing mechanism that is insufficient for our project.
This affects many bazel tests and potentially any bazel runnables.

To work around this issue, you can run the following bash script:

```bash
bash actions/unblock_user_namespace_for_linux_sandbox/action_callable.sh
```

Note. To take this into effect force, a bazel restart by shutting it down 1st

```bash
bazel shutdown
```

These steps must be rerun whenever the bazel version is updated.

### Build Instructions

To build and test the Communication Module, follow the steps below from the project root:

```bash
# Build all targets
bazel build //...

# Run all tests
bazel test //...
```

### Linting Instructions

For Linting the Code following solutions are available:

Copyright Checker:
```bash
# Check Sources
bazel run //:copyright.check

# Fix Sources
bazel run //:copyright.fix
```

C++ and Bazel files formatter:
```bash
# Check Sources
bazel run //:format.check

# Fix Sources
bazel run //:format
```

### Testing Instructions

The Communication Module includes extensive tests. Use the following commands:

#### Run All Tests
```bash
bazel test //...
```

#### Run Specific Test Suites
```bash
bazel test //score/mw/com/impl:all
bazel test //score/mw/com/message_passing:all
```

#### Test Categories
- **Unit Tests**: Component-level testing
- **Integration Tests**: Cross-component interactions
- **Performance Tests**: Latency and throughput benchmarks
- **Safety Tests**: ASIL-B compliance verification

## Additional Resources

For project details, documentation, and support resources, please refer to the main [README.md](README.md).

**Thank you for contributing to the Eclipse SCORE Communication Module!**
