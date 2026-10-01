# Contributing

## C++ Style

Project-owned C++ code uses the repository `.clang-format` file and the C++14
language standard. The formatting scope includes:

- `src/`
- `daemon/`
- `examples/someip/`
- `tests/`
- `tools/`

The `third_party/` tree is excluded from formatting and should retain the
upstream project's style. Build directories and generated files are excluded
as well.

### Source Organization

- `src/osabstraction/` and `src/someip/` are the two core library source roots;
  their child directories are intentionally owned by the parent CMake files.
- `src/serialization/` and `src/someip/exception/` contain private header-only
  utilities. They are consumed by the parent core targets and therefore do not
  define independent CMake targets.
- Platform-specific sources, such as QNX message-passing code and Linux epoll
  code, are selected or excluded by the parent `src/osabstraction/CMakeLists.txt`.
- `daemon/`, `tools/`, `examples/`, and `tests/` are separate executable or
  validation surfaces and are added directly by the root `CMakeLists.txt`.

### Naming

- Types, classes, and structs use `PascalCase`, for example `ServiceInfo`.
- Project-owned functions and methods use `snake_case`, for example
  `register_message_handler()` and `is_service_available()`.
- Constructors, destructors, operators, and required overrides keep the syntax
  required by C++ or their base/interface contract.
- The public `vsomeip` interface retains its upstream naming as an external API;
  its integration implementation calls the LG SOME/IP API using `snake_case`.
- Local variables and parameters use `snake_case`, including protocol
  abbreviations: `service_id`, `event_id`, and `ipv6_address`.
- Private non-static data members use `snake_case_`, for example
  `service_id_`.
- New typed constants use `kPascalCase`; macros retain an uppercase,
  project-prefixed form.
- Constants and enum values that mirror an external SOME/IP wire specification
  or JSON/protocol field name may retain that specification spelling. Internal
  aliases for those values should still use `kPascalCase`.
- Header guards use an `LG_SOMEIP_` prefix. Do not introduce identifiers that
  begin with `__` or `_` followed by an uppercase letter.
- Do not use leading-underscore parameters or `m`-prefixed private members.

Use `clang-format` 18 or a compatible newer release. Format project-owned C++
files with:

```sh
find src daemon examples/someip tests tools -type f \
    \( -name '*.h' -o -name '*.hpp' -o -name '*.cpp' -o -name '*.cc' \) \
    -print0 | xargs -0 clang-format -i
```

Check formatting without changing files:

```sh
find src daemon examples/someip tests tools -type f \
    \( -name '*.h' -o -name '*.hpp' -o -name '*.cpp' -o -name '*.cc' \) \
    -print0 | xargs -0 clang-format --dry-run --Werror
```

## Static Analysis

The repository `.clang-tidy` file enables a small set of bug-prone and
performance checks. Generate a compile database and run the checks on
project-owned files only:

```sh
cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
clang-tidy -p build src/someip/config/Configuration.cpp
```

Static-analysis findings should be reviewed individually. Do not apply broad
modernization changes to the public `vsomeip` API or vendor-derived code as part
of a formatting-only change.

The identifier-naming check covers functions, methods, parameters, local
variables, and private/protected members. Global variables, types, constants,
and enum values are reviewed manually because SOME/IP wire names, protocol
aliases, and external API spellings are documented exceptions.

## GitHub Workflow

Create focused branches and open pull requests against `main`. Pull requests
must include the relevant tests and explain configuration or protocol changes.
Do not commit build directories, coverage output, generated CMake files, or
local IDE metadata. GitHub Actions checks formatting and static analysis,
builds and tests the default configuration, builds the optional IPC and TLS
configurations, and builds and tests packet filtering. QNX builds are not part
of CI; maintainers validate them separately.

Releases use `MAJOR.MINOR.PATCH` versions from the root `CMakeLists.txt`. A
release should update [CHANGELOG.md](CHANGELOG.md), create a matching Git tag,
and attach the source archive and relevant build notes to the GitHub release.

## Validation

Keep formatting-only changes separate from behavior changes. At minimum, run:

```sh
git diff --check
cmake --build build
```

When test dependencies are available, enable both the project test option and
CTest's standard `BUILD_TESTING` option:

```sh
cmake -S . -B build-tests \
  -DENABLE_TEST_BUILD=ON \
  -DBUILD_TESTING=ON
cmake --build build-tests --parallel
ctest --test-dir build-tests --output-on-failure
```

On Linux, `tests/run_unit_tests_linux.sh` performs this configuration and also
generates coverage artifacts when `lcov` and `genhtml` are installed.