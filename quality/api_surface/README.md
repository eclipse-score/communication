# API Surface Stability Check

Ensures the public C++ API of a library doesn't change accidentally.
Uses **clang's AST dump** (`-Xclang -ast-dump=json`) for robust C++ parsing that
correctly handles preprocessor directives, macros, templates, ifdefs, and
conditional compilation.

## How it works

1. A Bazel build action runs `clang -ast-dump=json` on the target headers (in the
   proper sandbox with all include paths from CcInfo)
2. A Python tool (`extract_api.py`) parses the JSON AST and extracts public symbols
3. The extracted API surface is compared against a committed lock file (JSON)
4. If the API has changed, the test fails with a clear diff
5. An implicit `<name>.update` target allows intentional API changes to be committed

## Quick Start

### Run the API surface test

```bash
bazel test //score/mw/com:api_surface_test
```

### Update the lock file after intentional API changes

```bash
bazel run //score/mw/com:api_surface_test.update
```

### Check for undocumented public symbols

```bash
bazel test //score/mw/com:api_surface_docs_test
```

## Adding API Surface Checks to a New Target

1. Add the load statement to your `BUILD` file:

```python
load("//quality/api_surface:api_surface.bzl", "api_surface_test")
```

2. Add the test target (an implicit `<name>.update` target is created automatically):

```python
api_surface_test(
    name = "api_surface_test",
    target = ":your_cc_library",
    check_docs = False,  # Set True to enforce \api documentation
    lock_file = "api_surface.lock.json",
)
```

3. Generate the initial lock file:

```bash
bazel run //your/target:api_surface_test.update
```

4. Commit the generated `api_surface.lock.json`.

## Rule Attributes

### `api_surface_test`

| Attribute       | Type     | Description                                                  |
|-----------------|----------|--------------------------------------------------------------|
| `target`        | label    | cc_library target whose transitive direct public headers form the API |
| `lock_file`     | label    | The committed JSON lock file to compare against              |
| `check_docs`    | bool     | If true, fail when public symbols lack `\api` documentation  |

An implicit `<name>.update` runnable target is automatically created alongside every
`api_surface_test`. Run it with `bazel run` to regenerate the lock file after an
intentional API change.

## How Public Symbols Are Identified

The tool tracks:
- `using` / `typedef` declarations
- `class` / `struct` declarations (non-forward)
- `enum` / `enum class` declarations
- Free functions at namespace scope
- Public methods in class bodies
- Template variants of all the above

Symbols in `detail`, `internal`, or `impl` namespaces are automatically excluded.

## Documentation Enforcement

When `check_docs = True`, the tool verifies that every public symbol has:
- A `\api` (or `@api`) doxygen marker
- A `\brief` (or `@brief`) description

This ensures the public API is fully documented for end-users.
