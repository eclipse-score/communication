# Compiler Warnings

This directory defines the compiler warning feature set for the `score_communication` project
and verifies that each warning flag is correctly wired up in the Bazel toolchain.

## Overview

Warning flags are grouped into named `cc_feature` targets that consumers apply via the
`features` attribute of any `cc_binary`, `cc_library`, or `cc_test` rule. Features are
layered: each level implies all levels below it.

```
score_communication_minimal_warnings
  └─ score_communication_strict_warnings
       └─ score_communication_additional_warnings
```

`score_communication_additional_warnings` sits on top of `score_communication_strict_warnings`
(which it implies) and adds a further set of diagnostics of its own: `-Wcast-align`,
`-Wcast-qual`, `-Wundef`, `-Wwrite-strings`, and `-Wredundant-decls`.

`score_communication_treat_warnings_as_errors` (`-Werror`) is a separate orthogonal feature
that can be combined with any of the above.

`score_communication_third_party_warnings` is mutually exclusive with all other warning
features and is intended for external code that cannot be modified.

`score_communication_strict_warnings_no_error` is a convenience variant of strict warnings
that demotes selected diagnostics from errors to warnings — useful during incremental
adoption of stricter flags.

## Feature Reference

### Common flags ([`//quality/compiler_warnings:…`](../BUILD))

Shared base args consumed by both GCC and Clang features.

| `cc_args` target | Flags |
|-----------------|-------|
| `default_compile_args` | `-march=nehalem`, `-fstack-protector-strong`, `-fno-omit-frame-pointer`, `-fPIC`, `-D_FORTIFY_SOURCE=2`, `-D_GLIBCXX_ASSERTIONS`, `-fstack-clash-protection`, `-fcf-protection=full`, `-fdiagnostics-color=always` |
| `default_link_args` | `-pipe`, `-Wl,-z,relro,-z,now,-z,noexecstack,-z,notext`, `-fuse-ld=gold`, plus hardening/link flags (`-lrt`/`-latomic` are provided by the toolchain `link_libs`) |
| `minimal_warnings_args` | `-Wall`, `-Wno-error=cpp`, `-Wno-error=deprecated-declarations`, `-Wno-unused-macros`, `-Wno-unused-parameter`, `-Wno-unused-variable`, `-Wunused-but-set-parameter` |
| `strict_warnings_args` | `-Wextra`, `-pedantic`, `-Wconversion`, `-Wsign-conversion`, `-Wfloat-conversion`, `-Wfloat-equal`, `-Wformat=2`, `-Wshadow`, `-Wformat-security`, `-Wunused-macros`, `-Wmultichar`, `-Wpacked`, `-Winvalid-pch` |
| `additional_warnings_args` | `-Wcast-align`, `-Wcast-qual`, `-Wundef`, `-Wwrite-strings`, `-Wredundant-decls` |
| `treat_warnings_as_errors_args` | `-Werror` |

### GCC-specific flags ([`//quality/compiler_warnings/gcc:…`](../gcc/BUILD))

Additional flags applied only when the GCC toolchain is active.

| Feature | Extra flags |
|---------|-------------|
| `minimal_warnings` | `-Wno-builtin-macro-redefined`, `-Wno-maybe-uninitialized`, `-Wno-format-y2k`, `-Wno-free-nonheap-object`; C++ only: `-Wno-literal-suffix`, `-Wno-noexcept-type` |
| `strict_warnings` | `-Warray-bounds=2`, `-Wdisabled-optimization`, `-Wimplicit-fallthrough=4`, `-Wmissing-format-attribute`, `-Wscalar-storage-order`, `-Wsuggest-attribute=format`, `-Wvector-operation-performance`, `-Wlogical-op`; C++ only: `-Wdelete-non-virtual-dtor`, `-Woverloaded-virtual`, `-Wregister`, `-Wstrict-null-sentinel`; C only: `-Wold-style-definition`, `-Wstrict-prototypes` |
| `strict_warnings_no_error` | All of `strict_warnings` + `-Wno-error` suppressors for known GCC false positives (conversion, shadow, sign-compare, return-type, etc.) |
| `third_party_warnings` | `-w` (suppress all warnings) |

### Clang-specific flags ([`//quality/compiler_warnings/clang:…`](../clang/BUILD))

Additional flags applied only when the LLVM/Clang toolchain is active.

| Feature | Extra flags |
|---------|-------------|
| `minimal_warnings` | `-Wno-error=self-assign-overloaded`, `-Wno-return-type-c-linkage`, `-Wno-unused-command-line-argument`, `-Wno-deprecated-non-prototype` |
| `strict_warnings` | `-Warray-bounds`, `-Wimplicit-fallthrough`, `-Wno-error=shadow-uncaptured-local` (demoted — see [score_baselibs#304](https://github.com/eclipse-score/baselibs/issues/304)) |
| `strict_warnings_no_error` | All of `strict_warnings` + `-Wno-error` suppressors for known Clang false positives (conversion, shadow, sign-compare, return-type, etc.) |
| `third_party_warnings` | (no extra flags beyond mutual-exclusion) |

## Using Warning Features in BUILD Files

```python
cc_library(
    name = "my_lib",
    srcs = ["my_lib.cpp"],
    hdrs = ["my_lib.h"],
    features = [
        "score_communication_strict_warnings",
        "score_communication_treat_warnings_as_errors",
    ],
)
```

The full recommended set for production targets is available via the `COMPILER_WARNING_FEATURES`
constant defined in [`score/mw/common_features.bzl`](../../../score/mw/common_features.bzl):

```python
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "my_lib",
    srcs = ["my_lib.cpp"],
    features = COMPILER_WARNING_FEATURES,
)
```

> **Note:** `cc_feature` targets only work with the **LLVM toolchain**. The default GCC toolchain
> does not support them. To test features with LLVM explicitly:

```bash
bazel build //... \
  --extra_toolchains=@llvm_toolchain//:cc-toolchain-x86_64-linux
```

---

## Tests (`test/`)

The test suite verifies that every warning flag is correctly wired: flags that should
trigger do trigger, and clean code compiles without false positives.

### Test Strategy

| Case | Bazel target | Setup | Expected outcome |
|------|-------------|-------|-----------------|
| **True-positive build** | `tp_build_test` | Per-flag `has_warnings/<flag>.cpp` with `treat_warnings_as_errors` disabled | Compiles successfully — warnings are visible but not fatal |
| **False-positive build** | `fp_build_test` | `no_warnings/clean.cpp` with all `COMPILER_WARNING_FEATURES` enabled | Compiles successfully — no spurious warnings |
| **Negative (must-fail)** | `warnings_negative_test` | Per-flag `has_warnings/<flag>.cpp` compiled with `-Werror` + the specific flag | Build fails with the expected `-Werror=<flag>` diagnostic |

Each warning flag has its own source file and its own Bazel target so that every flag
is exercised independently. A single combined target would only surface the first
triggering flag and hide the rest.

### Test Files

| Path | Purpose |
|------|---------|
| [`no_warnings/clean.cpp`](no_warnings/clean.cpp) | False-positive baseline — clean code that must compile under all warning features |
| [`has_warnings/`](has_warnings/) | True-positive baselines — one `.cpp` per flag, each containing exactly one defect |
| [`BUILD`](BUILD) | Defines all three test families |

### Warning Coverage

| Source file | Warning flag | Bazel feature | MISRA / CWE |
|-------------|-------------|---------------|-------------|
| [has_warnings/conversion.cpp](has_warnings/conversion.cpp) | `-Wfloat-conversion` | `score_communication_strict_warnings` | MISRA Rule 7.0.5, CWE-681 |
| [has_warnings/shadow.cpp](has_warnings/shadow.cpp) | `-Wshadow` | `score_communication_strict_warnings` | MISRA Rule 6.4.1, CWE-398 |
| [has_warnings/sign_compare.cpp](has_warnings/sign_compare.cpp) | `-Wsign-compare` | `score_communication_strict_warnings` | CWE-195, CWE-697 |
| [has_warnings/delete_non_virtual_dtor.cpp](has_warnings/delete_non_virtual_dtor.cpp) | `-Wdelete-non-virtual-dtor` | `score_communication_strict_warnings` (C++) | MISRA Rule 15.7.1 |
| [has_warnings/overloaded_virtual.cpp](has_warnings/overloaded_virtual.cpp) | `-Woverloaded-virtual` | `score_communication_strict_warnings` (C++) | MISRA Rule 10.2.0 |
| [has_warnings/cast_qual.cpp](has_warnings/cast_qual.cpp) | `-Wcast-qual` | `score_communication_strict_warnings` | MISRA Rule 8.2.3 |
| [has_warnings/undef.cpp](has_warnings/undef.cpp) | `-Wundef` | `score_communication_strict_warnings` | MISRA Dir 4.4 |

`no_warnings/clean.cpp` demonstrates the correct coding patterns that avoid each of the
above warnings: `static_cast<>`, guarded comparisons, `[[maybe_unused]]`, distinct variable
names, const-safe pointer access, virtual destructors, and `using` declarations.

### Running the Tests

```bash
# Verify that warning code compiles when -Werror is disabled (true-positive build test).
bazel test //quality/compiler_warnings/test:tp_build_test

# Verify that clean code compiles with all warning features enabled (false-positive build test).
bazel test //quality/compiler_warnings/test:fp_build_test

# Run both build tests together.
bazel test //quality/compiler_warnings/test:fp_build_test \
          //quality/compiler_warnings/test:tp_build_test

# Verify that warning code fails to build with -Werror (negative / must-fail tests).
bazel test //quality/compiler_warnings/test:warnings_negative_test --test_output=errors
```
