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
  their child directories are intentionally owned by the `BUILD` file of the
  parent directory.
- `src/serialization/` and `src/someip/exception/` contain private header-only
  utilities. They are exposed by header-only targets in the root `BUILD` file and
  in `src/someip/BUILD` and do not have `BUILD` files of their own.
- Platform-specific sources, such as QNX message-passing code and Linux epoll
  code, are selected or excluded by `src/osabstraction/BUILD`.
- `daemon/`, `tools/`, `examples/`, `third_party/`, and `tests/` define their
  own targets in their `BUILD` files.
- Compile definitions, options, and features common to all targets are set by
  the `lgsomeip_cc_library`, `lgsomeip_cc_binary`, and `lgsomeip_cc_test`
  macros in `lgsomeip.bzl`. Use them instead of the plain `cc_*` rules.

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
project-owned files only. From the root of the `score_communication` workspace:

```sh
bazel run @hedron_compile_commands//:refresh_all
clang-tidy -p . score/lg-someip/src/someip/config/Configuration.cpp
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
Do not commit build output, coverage output, or local IDE metadata. lg-someip is
built and tested by the GitHub Actions workflows of the `score_communication`
repository, see the repository [CONTRIBUTING.md](../../CONTRIBUTING.md).

Releases use `MAJOR.MINOR.PATCH` versions from `LGSOMEIP_VERSION` in
[lgsomeip.bzl](lgsomeip.bzl). A release should update
[CHANGELOG.md](CHANGELOG.md), create a matching Git tag, and attach the source
archive and relevant build notes to the GitHub release.

## Validation

Keep formatting-only changes separate from behavior changes. At minimum, run:

```sh
git diff --check
bazel build //score/lg-someip/...
bazel test //score/lg-someip/...
```
