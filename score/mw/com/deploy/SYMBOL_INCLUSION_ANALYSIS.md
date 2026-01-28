# Symbol Inclusion Analysis: Why `score_com` Fails and `score_communication` Works

## Document Purpose

This document explains the critical difference between two shared library configurations in the SCORE Communication middleware:
- **`score_com`** - Does NOT work (missing symbols)
- **`score_communication`** - Works correctly (all symbols present)

The root cause is how Bazel's `cc_shared_library` rule handles symbol inclusion from transitive dependencies.

---

## Table of Contents

1. [Problem Statement](#problem-statement)
2. [Background: Bazel Symbol Inclusion](#background-bazel-symbol-inclusion)
3. [Configuration Comparison](#configuration-comparison)
4. [Why `score_com` Fails](#why-score_com-fails)
5. [Why `score_communication` Works](#why-score_communication-works)
6. [Symbol Visibility in Shared Libraries](#symbol-visibility-in-shared-libraries)
7. [Verification and Testing](#verification-and-testing)
8. [Best Practices](#best-practices)
9. [References](#references)

---

## Problem Statement

### The Issue

When building a shared library (`libscore_communication.so`) for the SCORE Communication middleware, we encountered a critical problem:

**Symptom**: Applications linking against the shared library fail with **undefined symbol errors** at runtime, even though:
- The code compiles successfully
- The dependencies are correctly specified
- The static library build works fine

**Root Cause**: Bazel's `cc_shared_library` rule only includes symbols from **direct dependencies** by default, not from **transitive dependencies**.

### Impact

This affects:
- Dynamic loading scenarios where applications link against `libscore_communication.so`
- Runtime symbol resolution
- Deployment packages that bundle the shared library
- Cross-platform builds (QNX, Linux, etc.)

---

## Background: Bazel Symbol Inclusion

### How Bazel Builds Shared Libraries

When Bazel creates a shared library using `cc_shared_library`, it follows these rules:

1. **Direct Dependencies**: Symbols from libraries listed directly in `deps` are included
2. **Transitive Dependencies**: Symbols from transitive dependencies are **NOT automatically included**
3. **Symbol Visibility**: Only symbols explicitly exported are visible to consumers

### The Transitive Dependency Problem

Consider this dependency chain:

```
Application
    └─> libscore_communication.so
            └─> com:runtime (direct dep)
                    └─> impl:runtime (transitive dep)
                            └─> impl:proxy_base (transitive dep)
                                    └─> impl:skeleton_base (transitive dep)
```

**Problem**: If the application directly calls functions from `impl:proxy_base`, but `libscore_communication.so` only lists `com:runtime` as a direct dependency, the symbols from `impl:proxy_base` will be **missing** from the shared library.

### Why This Happens

Bazel's design philosophy:
- **Explicit is better than implicit**: You must explicitly declare what you want
- **Minimal linking**: Only link what's directly needed
- **Avoid symbol bloat**: Don't automatically include everything

This works well for static libraries but causes issues for shared libraries that need to expose a complete API surface.

---

## Configuration Comparison

### Configuration 1: `score_com` (DOES NOT WORK)

```python
cc_library(
    name = "com_deps",
    alwayslink = True,
    deps = [
        # Only top-level API layer
        "//score/mw/com:com_error_domain",
        "//score/mw/com:runtime",
        "//score/mw/com:runtime_configuration",
        "//score/mw/com:types",
    ],
)

cc_shared_library(
    name = "score_com",
    deps = [":com_deps"],
    dynamic_deps = [],
)
```

**What's included**: Only symbols from the 4 direct dependencies
**What's missing**: All implementation layer symbols (proxy, skeleton, plumbing, bindings, etc.)

### Configuration 2: `score_communication` (WORKS)

```python
cc_library(
    name = "com_all_deps",
    alwayslink = True,
    deps = [
        # Public API layer
        "//score/mw/com:com_error_domain",
        "//score/mw/com:runtime",
        "//score/mw/com:runtime_configuration",
        "//score/mw/com:types",
        
        # Implementation layer - ALL transitive deps explicitly listed
        "//score/mw/com/impl:impl",
        "//score/mw/com/impl:runtime",
        "//score/mw/com/impl:proxy_base",
        "//score/mw/com/impl:proxy_event",
        "//score/mw/com/impl:skeleton_base",
        "//score/mw/com/impl:skeleton_event",
        # ... 30+ more implementation targets
        
        # Plumbing layer
        "//score/mw/com/impl/plumbing:plumbing",
        "//score/mw/com/impl/plumbing:runtime",
        
        # Tracing
        "//score/mw/com/impl/tracing:tracing_runtime",
        "//score/mw/com/impl/tracing:skeleton_event_tracing",
        "//score/mw/com/impl/tracing:proxy_event_tracing",
        
        # Bindings
        "//score/mw/com/impl/bindings/lola:lola",
        "//score/mw/com/impl/bindings/lola:proxy",
        "//score/mw/com/impl/bindings/lola:skeleton",
        
        # External dependencies
        "@score_baselibs//score/language/futurecpp:futurecpp",
        "@score_logging//score/mw/log",
    ],
)

cc_shared_library(
    name = "score_communication",
    deps = [":com_all_deps"],
    dynamic_deps = [],
)
```

**What's included**: ALL symbols from the entire dependency tree
**Result**: Complete API surface available to consumers

---

## Why `score_com` Fails

### The Dependency Graph

```
com_deps (aggregation library)
    ├─> com:runtime
    │       ├─> impl:runtime (TRANSITIVE - NOT INCLUDED)
    │       ├─> impl:proxy_base (TRANSITIVE - NOT INCLUDED)
    │       └─> impl:skeleton_base (TRANSITIVE - NOT INCLUDED)
    ├─> com:types
    │       └─> impl:types (TRANSITIVE - NOT INCLUDED)
    └─> com:runtime_configuration
            └─> impl:configuration (TRANSITIVE - NOT INCLUDED)
```

### What Happens at Build Time

1. **Compilation Phase**: ✅ Success
   - All headers are available
   - Code compiles without errors
   - Dependencies are resolved for compilation

2. **Linking Phase**: ✅ Success (but incomplete)
   - Bazel creates `libscore_com.so`
   - Only includes symbols from direct deps (4 targets)
   - Transitive deps are **not** included in the shared library

3. **Symbol Export**: ❌ Incomplete
   - Only ~25% of required symbols are exported
   - Implementation layer symbols are missing

### What Happens at Runtime

When an application tries to use the shared library:

```cpp
// Application code
#include "score/mw/com/runtime.h"
#include "score/mw/com/impl/proxy_base.h"  // Needs symbols from impl layer

int main() {
    // This works - symbol from direct dep
    auto runtime = score::mw::com::Runtime::Create();
    
    // This FAILS - symbol from transitive dep NOT in .so
    auto proxy = score::mw::com::impl::ProxyBase::Create();
    //           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    //           undefined symbol: _ZN5score2mw3com4impl9ProxyBase6CreateEv
}
```

**Error**:
```
./app: symbol lookup error: ./app: undefined symbol: _ZN5score2mw3com4impl9ProxyBase6CreateEv
```

### Why `alwayslink = True` Doesn't Help

The `alwayslink = True` attribute on `com_deps` only affects:
- Whether the library is linked even if no symbols are referenced
- Ensures static initializers run

It does **NOT**:
- Force inclusion of transitive dependencies
- Change Bazel's symbol inclusion rules
- Affect what goes into the shared library

---

## Why `score_communication` Works

### The Complete Dependency Graph

```
com_all_deps (comprehensive aggregation)
    ├─> com:runtime (DIRECT)
    ├─> impl:runtime (DIRECT - explicitly added)
    ├─> impl:proxy_base (DIRECT - explicitly added)
    ├─> impl:proxy_event (DIRECT - explicitly added)
    ├─> impl:skeleton_base (DIRECT - explicitly added)
    ├─> impl:skeleton_event (DIRECT - explicitly added)
    ├─> impl/plumbing:plumbing (DIRECT - explicitly added)
    ├─> impl/plumbing:runtime (DIRECT - explicitly added)
    ├─> impl/tracing:tracing_runtime (DIRECT - explicitly added)
    ├─> impl/bindings/lola:lola (DIRECT - explicitly added)
    ├─> impl/bindings/lola:proxy (DIRECT - explicitly added)
    ├─> impl/bindings/lola:skeleton (DIRECT - explicitly added)
    └─> ... (30+ more implementation targets)
```

### Key Difference: Flattened Dependency Tree

Instead of relying on transitive dependencies, we **explicitly list every required target** as a direct dependency.

**Before (transitive)**:
```python
deps = [
    "//score/mw/com:runtime",  # This brings in impl:runtime transitively
]
```

**After (direct)**:
```python
deps = [
    "//score/mw/com:runtime",      # API layer
    "//score/mw/com/impl:runtime",  # Implementation layer - EXPLICIT
]
```

### Symbol Inclusion Process

1. **Compilation Phase**: ✅ Success
   - All headers available
   - All dependencies resolved

2. **Linking Phase**: ✅ Complete
   - Bazel creates `libscore_communication.so`
   - Includes symbols from **all 40+ direct dependencies**
   - All implementation symbols are present

3. **Symbol Export**: ✅ Complete
   - 100% of required symbols exported
   - Complete API surface available

### Runtime Behavior

```cpp
// Application code
#include "score/mw/com/runtime.h"
#include "score/mw/com/impl/proxy_base.h"

int main() {
    // Both work - all symbols present in .so
    auto runtime = score::mw::com::Runtime::Create();  ✅
    auto proxy = score::mw::com::impl::ProxyBase::Create();  ✅
}
```

**Result**: Application runs successfully, all symbols resolved.

---

## Symbol Visibility in Shared Libraries

### Symbol Versioning Script

Both configurations use a version script to control symbol visibility:

```lds
SCORE_COM_1.0 {
    global:
        # Export all score symbols
        *score*;
    local:
        # Hide all other symbols
        *;
};
```

This ensures:
- All SCORE symbols are exported (public API)
- Internal symbols are hidden (implementation details)
- Symbol versioning for ABI compatibility

### Linker Flags

```python
user_link_flags = select({
    ":qnx8": [
        "-Wl,-soname,libscore_communication.so",
        "-Wl,-rpath,'$$ORIGIN'",
        "-Wl,--version-script=$(location :generate_version_script)",
        "-lslog2",  # QNX system library
        "-lfsnotify",  # QNX8-specific
        "-Wl,--allow-multiple-definition",
        "-static-libgcc",
        "-static-libstdc++",
    ],
})
```

**Key flags**:
- `--version-script`: Controls symbol visibility
- `--allow-multiple-definition`: Handles duplicate symbols
- `-static-libgcc/libstdc++`: Statically link standard libraries
- `-lslog2/-lfsnotify`: Dynamically link QNX system libraries

### Static vs Dynamic Linking

```python
cc_shared_library(
    name = "score_communication",
    deps = [":com_all_deps"],
    dynamic_deps = [],  # Empty = everything static except system libs
)
```

**`dynamic_deps = []`** means:
- All SCORE dependencies are statically linked into the `.so`
- Only system libraries (slog2, fsnotify, acl) are dynamically linked
- Self-contained shared library

---

## Verification and Testing

### Check Symbols in Shared Library

```bash
# List all exported symbols
nm -D libscore_communication.so | grep " T " | c++filt

# Check for specific symbol
nm -D libscore_communication.so | grep ProxyBase | c++filt

# Compare two libraries
nm -D libscore_com.so | wc -l          # Fewer symbols
nm -D libscore_communication.so | wc -l  # More symbols
```

### Verify Symbol Presence

```bash
# Check if a specific symbol exists
objdump -T libscore_communication.so | grep "ProxyBase::Create"

# List undefined symbols (should be minimal - only system libs)
nm -u libscore_communication.so
```

### Test Application Linking

```bash
# Build test application
bazel build //score/mw/com/deploy:ipc_bridge_dynamic --config=hqx_qnx8

# Check runtime dependencies
ldd bazel-bin/score/mw/com/deploy/ipc_bridge_dynamic

# Run on target
./ipc_bridge_dynamic -n 1 -m proxy -t 1000 -s mw_com_config.json
```

### Symbol Count Comparison

| Configuration | Exported Symbols | Coverage |
|---------------|------------------|----------|
| `score_com` | ~500 | 25% (API only) |
| `score_communication` | ~2000 | 100% (API + Implementation) |

---

## Best Practices

### 1. Explicitly List All Required Dependencies

❌ **Don't rely on transitive dependencies**:
```python
deps = [
    "//score/mw/com:runtime",  # Assumes impl:runtime comes transitively
]
```

✅ **Explicitly list everything**:
```python
deps = [
    "//score/mw/com:runtime",
    "//score/mw/com/impl:runtime",  # Explicit
    "//score/mw/com/impl:proxy_base",  # Explicit
]
```

### 2. Use `alwayslink = True` for Aggregation Libraries

```python
cc_library(
    name = "com_all_deps",
    alwayslink = True,  # Force inclusion even if not directly referenced
    deps = [ ... ],
)
```

### 3. Document Why Each Dependency is Needed

```python
deps = [
    # Public API layer
    "//score/mw/com:runtime",
    
    # Implementation layer - needed for ProxyBase symbols
    "//score/mw/com/impl:proxy_base",
    
    # Bindings - needed for LOLA IPC implementation
    "//score/mw/com/impl/bindings/lola:lola",
]
```

### 4. Test Symbol Completeness

Create a test that verifies all expected symbols are present:

```python
sh_test(
    name = "verify_symbols_test",
    srcs = ["verify_symbols.sh"],
    data = [":score_communication"],
)
```

```bash
#!/bin/bash
# verify_symbols.sh
REQUIRED_SYMBOLS=(
    "score::mw::com::Runtime::Create"
    "score::mw::com::impl::ProxyBase::Create"
    "score::mw::com::impl::SkeletonBase::Create"
)

for symbol in "${REQUIRED_SYMBOLS[@]}"; do
    if ! nm -D libscore_communication.so | grep -q "$symbol"; then
        echo "ERROR: Missing symbol: $symbol"
        exit 1
    fi
done
```

### 5. Use Symbol Versioning

Always use a version script to control symbol visibility:

```python
user_link_flags = [
    "-Wl,--version-script=$(location :generate_version_script)",
]
```

### 6. Document the Dependency Structure

Maintain a document (like this one) explaining:
- Why certain dependencies are needed
- What symbols they provide
- Why the configuration works

---

## Header Extraction Challenges

### The Public Header Problem

In addition to the symbol inclusion issue, there's another challenge when creating deployment packages: **extracting the correct public headers** for consumers of the shared library.

### Problem Statement

The SCORE Communication project does not have:
- Clear separation between public and private headers
- Explicit `visibility` rules for headers
- A dedicated "public API" header package
- Documentation of which headers are part of the public interface

**Result**: When creating a deployment package with `libscore_communication.so`, we need to determine which headers to include, but there's no explicit definition of what constitutes the "public API".

### Why This is Challenging

#### 1. Mixed Public/Private Headers

Many targets mix public and private headers:

```python
cc_library(
    name = "proxy_base",
    hdrs = [
        "proxy_base.h",           # Public? Private?
        "proxy_base_impl.h",      # Definitely private
        "proxy_base_internal.h",  # Definitely private
    ],
    srcs = [
        "proxy_base.cpp",
    ],
)
```

**Problem**: Bazel's `CcInfo.compilation_context.headers` includes ALL headers (both public and private) needed for compilation.

#### 2. No Explicit Public API Definition

Unlike well-structured projects that have:

```python
# Good example (not in our codebase)
cc_library(
    name = "public_api",
    hdrs = glob(["include/score/mw/com/*.h"]),  # Only public headers
    visibility = ["//visibility:public"],
)

cc_library(
    name = "impl",
    hdrs = glob(["impl/*.h"]),  # Private headers
    visibility = ["//score/mw/com:__subpackages__"],  # Restricted
)
```

Our codebase has:

```python
# Current structure
cc_library(
    name = "com",
    hdrs = glob(["**/*.h"]),  # Everything mixed together
    visibility = ["//visibility:public"],
)
```

#### 3. Transitive Header Dependencies

Consumers need headers from:
- Direct dependencies (e.g., `com:runtime`)
- Transitive dependencies (e.g., `impl:proxy_base`)
- External dependencies (e.g., `futurecpp`, `logging`)

But there's no clear boundary of what's "public" vs "implementation detail".

### The Header Extraction Solution

Since we can't rely on explicit public header definitions, we use a **header extraction rule** that collects headers from the compilation context.

#### Implementation: `extract_headers` Rule

**File**: `communication/score/mw/com/deploy/extract_headers_for_deps.bzl`

```python
def _extract_public_headers_impl(ctx):
    """Extract headers from CcInfo provider."""
    
    all_headers = []
    
    for dep in ctx.attr.deps:
        if CcInfo not in dep:
            continue
        
        cc_info = dep[CcInfo]
        compilation_context = cc_info.compilation_context
        
        # Extract ALL headers needed for compilation
        # This includes public, private, and transitive headers
        all_headers.append(compilation_context.headers)
    
    headers = depset(transitive = all_headers)
    return [DefaultInfo(files = headers)]

extract_headers = rule(
    implementation = _extract_public_headers_impl,
    attrs = {
        "deps": attr.label_list(
            providers = [CcInfo],
            doc = "cc_library targets to extract headers from",
        ),
    },
)
```

#### Usage in BUILD File

```python
# Extract headers from key dependencies
extract_headers(
    name = "score_com_public_headers",
    deps = [
        "//score/mw/com:com",                      # Main API
        "@score_baselibs//score/language/futurecpp",  # External dep
        "@score_baselibs//score/mw/log",              # External dep
    ],
)
```

### What Gets Extracted

The `extract_headers` rule collects:

1. **Direct Headers**: Headers from the specified targets
2. **Transitive Headers**: Headers from all transitive dependencies
3. **External Headers**: Headers from external dependencies (boost, baselibs, etc.)

**Example output**:
```
score/mw/com/runtime.h
score/mw/com/types.h
score/mw/com/impl/proxy_base.h
score/mw/com/impl/skeleton_base.h
score/language/futurecpp/expected.hpp
score/mw/log/logging.h
boost/program_options.hpp
nlohmann/json.hpp
... (hundreds more)
```

### The Over-Inclusion Problem

Because we extract from `compilation_context.headers`, we get:

✅ **All necessary public headers**  
✅ **Headers needed by consumers**  
❌ **Private implementation headers** (not needed by consumers)  
❌ **Third-party headers** (boost, nlohmann, etc.)  
❌ **Internal detail headers** (impl, detail, internal directories)  

### Filtering Solution: `copy_headers` Rule

To address over-inclusion, we use a second rule that filters headers during the copy phase.

**File**: `qcipc/src/com_dynamic/copy_headers.bzl`

```python
def _copy_headers_impl(ctx):
    """Copy headers with filtering."""
    
    # Define top-level directories to exclude
    exclude_top_level_dirs = [
        "nlohmann",  # Third-party JSON library
        "boost",     # Third-party boost libraries
        "visitor",   # Internal serialization details
    ]
    
    input_headers = ctx.attr.headers.files.to_list()
    output_headers = []
    
    for hdr in input_headers:
        # Strip external repository prefixes
        relative_path = strip_external_prefix(hdr.short_path)
        
        # Get top-level directory
        top_level_dir = relative_path.split("/")[0]
        
        # Skip if in exclude list
        if top_level_dir in exclude_top_level_dirs:
            continue
        
        # Copy to output include/ directory
        output_file = ctx.actions.declare_file("include/" + relative_path)
        ctx.actions.run_shell(
            inputs = [hdr],
            outputs = [output_file],
            command = "cp {} {}".format(hdr.path, output_file.path),
        )
        output_headers.append(output_file)
    
    return [DefaultInfo(files = depset(output_headers))]
```

### Two-Phase Approach

#### Phase 1: Extract (Broad)

```python
extract_headers(
    name = "score_com_public_headers",
    deps = ["//score/mw/com:com", ...],
)
```

**Result**: All headers from compilation context (~1000+ headers)

#### Phase 2: Filter & Copy (Narrow)

```python
copy_headers(
    name = "copied_headers",
    headers = ":score_com_public_headers",
)
```

**Result**: Filtered headers in `include/` directory (~500 headers)

### Why This Approach is Necessary

#### Alternative 1: Manual Header List ❌

```python
filegroup(
    name = "public_headers",
    srcs = [
        "runtime.h",
        "types.h",
        "impl/proxy_base.h",
        # ... manually list 500+ headers
    ],
)
```

**Problems**:
- Extremely tedious to maintain
- Easy to miss headers
- Breaks when new headers are added
- No way to track transitive dependencies

#### Alternative 2: Glob Patterns ❌

```python
filegroup(
    name = "public_headers",
    srcs = glob(["**/*.h", "**/*.hpp"]),
)
```

**Problems**:
- Includes private headers
- Includes test headers
- Includes internal implementation details
- No control over what's public vs private

#### Alternative 3: Separate Public API Package ❌

```python
cc_library(
    name = "public_api",
    hdrs = glob(["include/**/*.h"]),
)
```

**Problems**:
- Requires major refactoring of the codebase
- Need to move all public headers to `include/` directory
- Need to update all internal includes
- Breaking change for existing consumers

#### Our Approach: Extraction + Filtering ✅

**Advantages**:
- Works with existing code structure
- Automatically tracks dependencies
- Includes transitive headers
- Allows filtering of unwanted headers
- No code refactoring required
- Maintainable and scalable

### Limitations and Trade-offs

#### 1. Over-Inclusion Risk

**Issue**: We might include some private headers that happen to be in the compilation context.

**Mitigation**: 
- Filter known private directories (impl/detail, internal, etc.)
- Filter third-party headers (boost, nlohmann)
- Document which headers are truly public

#### 2. Include Path Challenges

**Issue**: Extracted headers lose their original include path information from `CcInfo`.

**Problem**:
```cpp
// Consumer code
#include "score/mw/log/logging.h"  // Can't find header!
```

**Solution**: Consumers must add dependencies for include paths:
```python
cc_binary(
    name = "my_app",
    deps = [
        ":score_com_dynamic",  # Provides .so and extracted headers
        "@score_logging//score/mw/log",  # Provides include paths
    ],
)
```

#### 3. Header-Only vs Implementation

**Issue**: Some headers are header-only (templates, inline functions) while others need implementation.

**Current Approach**: 
- All symbols are in `libscore_communication.so`
- Headers are just for compilation
- No need to distinguish header-only vs implementation

### Future Improvements

#### 1. Explicit Public API Definition

**Goal**: Refactor codebase to have clear public/private separation.

```python
# Future structure
cc_library(
    name = "public_api",
    hdrs = glob(["include/score/mw/com/*.h"]),
    visibility = ["//visibility:public"],
)

cc_library(
    name = "impl",
    hdrs = glob(["impl/**/*.h"]),
    visibility = ["//score/mw/com:__pkg__"],  # Private
    deps = [":public_api"],
)
```

#### 2. Header Visibility Rules

**Goal**: Use Bazel visibility to enforce public/private boundaries.

```python
cc_library(
    name = "internal_utils",
    hdrs = ["internal/utils.h"],
    visibility = ["//score/mw/com:__subpackages__"],  # Only internal
)
```

#### 3. Public Header Package

**Goal**: Create a dedicated package for public headers.

```python
# score/mw/com/public/BUILD
cc_library(
    name = "public_headers",
    hdrs = glob(["*.h"]),
    visibility = ["//visibility:public"],
)
```

#### 4. Documentation

**Goal**: Document which headers are part of the public API.

```cpp
// runtime.h
/**
 * @file runtime.h
 * @brief Public API for SCORE Communication Runtime
 * @public
 */
```

### Comparison with Other Projects

#### Well-Structured Project Example

**Abseil (Google)**:
```
abseil-cpp/
  absl/
    strings/          # Public API
      string_view.h   # Public header
      BUILD           # Exports only public headers
    strings/internal/ # Private implementation
      string_view_internal.h  # Private header
      BUILD           # Restricted visibility
```

**Our Current Structure**:
```
communication/
  score/mw/com/
    runtime.h         # Public? Private?
    impl/
      runtime_impl.h  # Public? Private?
    BUILD             # Everything mixed
```

### Best Practices for Header Management

#### 1. Use Header Extraction for Deployment

```python
# For deployment packages
extract_headers(
    name = "deployment_headers",
    deps = [":public_api"],
)
```

#### 2. Filter Unwanted Headers

```python
# Remove third-party and internal headers
copy_headers(
    name = "filtered_headers",
    headers = ":deployment_headers",
    exclude = ["boost", "nlohmann", "internal"],
)
```

#### 3. Document Public API

Maintain a list of public headers:
```markdown
# Public API Headers

- score/mw/com/runtime.h
- score/mw/com/types.h
- score/mw/com/proxy.h
- score/mw/com/skeleton.h
```

#### 4. Test Header Completeness

```python
sh_test(
    name = "verify_headers_test",
    srcs = ["verify_headers.sh"],
    data = [":copied_headers"],
)
```

### Summary

The header extraction methodology is necessary because:

1. **No explicit public API definition** in the codebase
2. **Mixed public/private headers** in the same targets
3. **Transitive dependencies** need to be tracked
4. **No clear visibility rules** for headers

Our solution:
- **Extract** all headers from compilation context
- **Filter** unwanted headers (third-party, internal)
- **Copy** to deployment package
- **Document** limitations and trade-offs

This is a pragmatic approach that works with the current codebase structure while acknowledging the need for future improvements.

---

## References

### Bazel Documentation

- [cc_shared_library](https://bazel.build/reference/be/c-cpp#cc_shared_library)
- [cc_library](https://bazel.build/reference/be/c-cpp#cc_library)
- [Dependency Management](https://bazel.build/concepts/dependencies)

### Related Files

- `communication/score/mw/com/deploy/BUILD` - Main configuration
- `communication/score/mw/com/deploy/SYMBOL_VERSIONING_GUIDE.md` - Symbol versioning details
- `qcipc/src/com_dynamic/BUILD` - Dynamic library usage example

### Symbol Visibility

- [GNU ld Version Scripts](https://sourceware.org/binutils/docs/ld/VERSION.html)
- [ELF Symbol Visibility](https://gcc.gnu.org/wiki/Visibility)

---

## Conclusion

### Summary

The difference between `score_com` (failing) and `score_communication` (working) boils down to:

**`score_com`**: Relies on transitive dependencies → Bazel doesn't include them → Missing symbols

**`score_communication`**: Explicitly lists all dependencies → Bazel includes them all → Complete symbol set

### Key Takeaway

**When building shared libraries in Bazel, you must explicitly list ALL dependencies whose symbols you want to export, not just the top-level API dependencies.**

This is counter-intuitive because:
- Static libraries work fine with transitive dependencies
- The code compiles successfully either way
- The problem only manifests at runtime

### Recommendation

Always use the `score_communication` pattern for shared libraries:
1. Create a comprehensive aggregation library (`com_all_deps`)
2. Explicitly list every target whose symbols are needed
3. Use `alwayslink = True` to ensure inclusion
4. Test symbol completeness before deployment

---

**Document Version**: 1.0  
**Last Updated**: 2025-01-XX  
**Author**: SCORE Communication Team  
**Related Issue**: Symbol inclusion in cc_shared_library
