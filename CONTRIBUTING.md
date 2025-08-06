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

To build and test the Communication Module, follow the steps below from the project root:

```bash
# Display project-specific help
bazel run //:help

# Build all targets
bazel build //...

# Run all tests
bazel test //...
```

Ensure you have Bazel and required tools installed as per project documentation.

## Code Formatting and Linting

Maintain consistency and code quality using the following tools:

### Format Code
```bash
clang-format -i $(find . -name "*.cpp" -o -name "*.h")
```

Or use Bazel format targets:
```bash
bazel test //:format.check        # Check formatting
bazel run //:format.fix           # Auto-fix formatting
```

### Lint Code (Static Analysis)
```bash
bazel build //... --config=clang-tidy
```

## Testing

The Communication Module includes extensive tests. Use the following commands:

### Run All Tests
```bash
bazel test //...
```

### Run Specific Test Suites
```bash
bazel test //score/mw/com/impl:all
bazel test //score/mw/com/message_passing:all
```

### Run with Coverage
```bash
bazel coverage //...
```

### Test Categories
- **Unit Tests**: Component-level testing
- **Integration Tests**: Cross-component interactions
- **Performance Tests**: Latency and throughput benchmarks
- **Safety Tests**: ASIL-B compliance verification

## Additional Resources

For project details, documentation, and support resources, please refer to the main [README.md](README.md).

**Thank you for contributing to the Eclipse SCORE Communication Module!** 
