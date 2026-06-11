# Compiler Warnings Test

Verifies that warning features work correctly using two complementary test cases.

## Test Strategy

| Case | Target | Features | Code | Expected result |
|------|--------|----------|------|-----------------|
| 1 | `tp_build_test` | `treat_warnings_as_errors` disabled | `has_warnings/<flag>.cpp` — one bug per flag | ✅ compiles (warnings visible, not fatal) |
| 2 | `fp_build_test` | all enabled (via `COMPILER_WARNING_FEATURES`) | `clean.cpp` — no bugs | ✅ compiles |
| 3 | `warnings_negative_test` | `-Werror` + per-flag warning option | `has_warnings/<flag>.cpp` — one bug per flag | ❌ build fails |

Each warning flag has its own source file and its own bazel target (`tp_<flag>`)
so every flag is exercised independently. A single combined target would only
surface the first flag that triggers, hiding the rest.

## Files

| File | Purpose |
|------|---------|
| [`no_warnings/clean.cpp`](no_warnings/clean.cpp) | False-positive baseline — must compile with all warning features enabled |
| [`has_warnings/`](has_warnings) | True-positive baseline — one source file per warning flag |
| [`BUILD`](BUILD) | Defines `fp_all`/`fp_build_test`, per-flag `tp_<flag>`/`tp_build_test`, and per-flag `fail_<flag>_build_error_test` targets |

## Running the Tests

```bash
# Case 1: faulty code compiles when treat_warnings_as_errors is disabled
bazel test //quality/compiler_warnings/test:tp_build_test

# Case 2: clean code compiles with all warning features enabled
bazel test //quality/compiler_warnings/test:fp_build_test

# Run both together
bazel test //quality/compiler_warnings/test:fp_build_test //quality/compiler_warnings/test:tp_build_test

# Case 3: buggy code must fail to compile (rules_build_error)
bazel test //quality/compiler_warnings/test:warnings_negative_test --test_output=all
```

> **Note:** `cc_feature` targets only work with the **LLVM toolchain**. The default GCC toolchain
> ignores them. To exercise features with LLVM:

```bash
# Explicitly switch to LLVM
bazel test //quality/compiler_warnings/test:fp_build_test \
  --extra_toolchains=@llvm_toolchain//:cc-toolchain-x86_64-linux
```

## Warning Coverage

### True Positives (`has_warnings/`)

One source file per flag, each triggering exactly one warning category:

| Source file | Warning flag | Feature | MISRA / CWE |
|-------------|-------------|---------|-------------|
| `conversion.cpp` | `-Wconversion` / `-Wfloat-conversion` | `score_communication_strict_warnings` | MISRA Rule 7.0.5, CWE-681 |
| `shadow.cpp` | `-Wshadow` | `score_communication_strict_warnings` | MISRA Rule 6.4.1, CWE-398 |
| `sign_compare.cpp` | `-Wsign-compare` | `score_communication_strict_warnings` | CWE-195, CWE-697 |
| `unused_parameter.cpp` | `-Wunused-parameter` | `score_communication_strict_warnings` | MISRA Rule 0.1.1, CWE-561 |
| `delete_non_virtual_dtor.cpp` | `-Wdelete-non-virtual-dtor` | `score_communication_strict_warnings` (C++) | MISRA Rule 15.7.1 |
| `overloaded_virtual.cpp` | `-Woverloaded-virtual` | `score_communication_strict_warnings` (C++) | MISRA Rule 10.2.0 |
| `cast_qual.cpp` | `-Wcast-qual` | `score_communication_additional_warnings` | MISRA Rule 8.2.3 |
| `format_nonliteral.cpp` | `-Wformat-nonliteral` | `score_communication_additional_warnings` | CWE-134 |
| `undef.cpp` | `-Wundef` | `score_communication_additional_warnings` | MISRA Dir 4.4 |

### False Positives (`clean.cpp`)

Demonstrates correct coding patterns that avoid triggering the above warnings:
`static_cast<>`, guarded comparisons, `std::ignore`, `[[maybe_unused]]`,
distinct variable names, const-safe access, literal format strings, defined macros,
virtual destructors, and `using` declarations.
