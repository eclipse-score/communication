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
5. An update target allows intentional API changes to be committed

## Quick Start

### Run the API surface test

```bash
bazel test //score/mw/com:api_surface_test
```

### Update the lock file after intentional API changes

```bash
bazel run //score/mw/com:api_surface_update
```

### Check for undocumented public symbols

```bash
bazel test //score/mw/com:api_surface_docs_test
```

## Adding API Surface Checks to a New Target

1. Add the load statement to your `BUILD` file:

```python
load("//quality/api_surface:api_surface.bzl", "api_surface_test", "api_surface_update")
```

2. Add the test and update targets:

```python
api_surface_test(
    name = "api_surface_test",
    hdrs = [
        "public_header_1.h",
        "public_header_2.h",
    ],
    check_docs = False,  # Set True to enforce \api documentation
    lock_file = "api_surface.lock.json",
    target_label = "//your/target:name",
)

api_surface_update(
    name = "api_surface_update",
    hdrs = [
        "public_header_1.h",
        "public_header_2.h",
    ],
    lock_file_path = "your/target/api_surface.lock.json",
    target_label = "//your/target:name",
)
```

3. Generate the initial lock file:

```bash
bazel run //your/target:api_surface_update
```

4. Commit the generated `api_surface.lock.json`.

## Rule Attributes

### `api_surface_test`

| Attribute       | Type     | Description                                                  |
|-----------------|----------|--------------------------------------------------------------|
| `hdrs`          | labels   | Header files that form the public API surface                |
| `deps`          | labels   | cc_library targets whose headers form the API (alternative)  |
| `lock_file`     | label    | The committed JSON lock file to compare against              |
| `check_docs`    | bool     | If true, fail when public symbols lack `\api` documentation  |
| `target_label`  | string   | Bazel target label (metadata only)                           |

### `api_surface_update`

| Attribute        | Type     | Description                                              |
|------------------|----------|----------------------------------------------------------|
| `hdrs`           | labels   | Header files that form the public API surface            |
| `deps`           | labels   | cc_library targets whose headers form the API            |
| `lock_file_path` | string   | Source-tree-relative path for the lock file              |
| `target_label`   | string   | Bazel target label (metadata only)                       |

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
