# Contributing

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

### Optional: Configure a local Ubuntu snapshot mirror

The Ubuntu snapshot mirrors are sometimes unreliable. See: https://answers.launchpad.net/launchpad/+question/824541

If you have access to a more reliable Ubuntu snapshot mirror, you can make Bazel try it first while keeping
the public `snapshot.ubuntu.com` URL as fallback.

An example for such a snapshot mirror service is https://stablebuild.com

1. Create a local downloader config at `user.downloader_config` (this file is ignored by git).
   Replace the `<private-mirror-tag>` with the tag provided by the StableBuild API

```text
rewrite snapshot.ubuntu.com/ubuntu/[^/]*/(.*) <private-mirror-tag>.debmirror.stablebuild.com/2026-04-01T10:40:01Z/archive.ubuntu.com/ubuntu/$1
rewrite snapshot.ubuntu.com/ubuntu/[^/]*/(.*) <private-mirror-tag>.debmirror.stablebuild.com/2026-04-01T10:40:01Z/ports.ubuntu.com/$1
rewrite snapshot.ubuntu.com/ubuntu/(.*) snapshot.ubuntu.com/ubuntu/$1
```

2. Point Bazel to this local config in `user.bazelrc` (also ignored by git):

```text
common --downloader_config=user.downloader_config
```

3. Restart the Bazel server to reload the downloader config

```shell
bazel shutdown
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

---

## How to Document

This section explains how to document C++ code so it appears in the generated API documentation.

> API documentation is generated using Doxygen and integrated via Breathe.
> Due to Sphinx version < 9.x limitations with complex C++ templates, use specific class
> and function directives rather than namespace-level documentation.

### Automatic API Documentation with @api Tag

The project includes an automated RST generation utility that extracts items
tagged with `@api` in Doxygen comments and generates categorized documentation.

Add the `@api` tag to any Doxygen comment block to include it in the generated API documentation:

```cpp
/**
 * @brief Connection handler for client communication
 * @api
 */
class ClientConnection {
    // ...
};

/**
 * @brief Initialize the communication system
 * @return true if successful
 * @api
 */
bool initialize();
```

The utility automatically creates:

- **API Index** — overview of all tagged items
- **Namespace Reference** — namespaces containing `@api` items
- **Class Reference** — classes and structs with `@api` tag
- **Member Reference** — functions and methods with `@api` tag

Build integration:

```starlark
generate_api_rst(
    name = "generate_api_rst",
    doxygen_xml = "//path/to:doxygen_target",
    output_dir = "generated",
    project_name = "mw::com",
)
```

The generated RST files are automatically included in the Sphinx documentation
under the "Generated API Documentation" section.

### Using Breathe Directives

To document specific classes or functions, use these Breathe directives in RST files:

**Document a specific class:**

```rst
.. doxygenclass:: score::message_passing::detail::ClientConnection
    :members:
    :protected-members:
    :undoc-members:
```

**Document a specific function:**

```rst
.. doxygenfunction:: score::mw::com::functionName
```

**Document a struct:**

```rst
.. doxygenstruct:: score::mw::com::ConfigStruct
    :members:
```

### Further Resources

- Browse the Doxygen-generated HTML at `bazel-bin/score/mw/com/design/doxygen_build/html/index.html`
- Review header files in `score/mw/com/` for inline documentation
- Check the design documentation at `score/mw/com/design/`

**Thank you for contributing to the Eclipse SCORE Communication Module!**
