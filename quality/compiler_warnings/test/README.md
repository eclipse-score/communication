# Compiler Warnings Test

Verifies that warning features work correctly using two complementary test cases.

## Test Strategy

| Case | Target | Features on `tp_all` | Code | Expected result |
|------|--------|----------------------|------|-----------------|
| 1 | `fp_build_test` | all enabled (via `COMPILER_WARNING_FEATURES`) | `clean.cpp` — no bugs | ✅ compiles |
| 2 | `tp_build_test` | `treat_warnings_as_errors` disabled | `has_warnings.cpp` — has bugs | ✅ compiles (warnings visible, not fatal) |
| 3 (future) | — | all enabled | `has_warnings.cpp` — has bugs | ❌ build fails |

## Files

| File | Purpose |
|------|---------|
| [`clean.cpp`](clean.cpp) | False-positive baseline — must compile with all warning features enabled |
| [`has_warnings.cpp`](has_warnings.cpp) | True-positive baseline — triggers 9 distinct warning categories |
| [`BUILD`](BUILD) | Defines `fp_all`, `fp_build_test`, `tp_all`, `tp_build_test` targets |

## Running the Tests

```bash
# Case 1: clean code compiles with all warning features enabled
bazel test //quality/compiler_warnings/test:fp_build_test

# Case 2: faulty code compiles when treat_warnings_as_errors is disabled
bazel test //quality/compiler_warnings/test:tp_build_test

# Run both together
bazel test //quality/compiler_warnings/test:fp_build_test //quality/compiler_warnings/test:tp_build_test
```

> **Note:** `cc_feature` targets only work with the **LLVM toolchain**. The default GCC toolchain
> ignores them. To exercise features with LLVM:

```bash
# Explicitly switch to LLVM
bazel test //quality/compiler_warnings/test:fp_build_test \
  --extra_toolchains=@llvm_toolchain//:cc-toolchain-x86_64-linux

# Sanitizer configs activate LLVM automatically
bazel test --config=asan_ubsan_lsan //quality/compiler_warnings/test:fp_build_test
bazel test --config=tsan            //quality/compiler_warnings/test:fp_build_test
```

## Warning Coverage

### True Positives (`has_warnings.cpp`)

9 functions, each triggering exactly one warning category:

| Function | Warning flag | Feature | MISRA / CWE |
|----------|-------------|---------|-------------|
| `implicit_narrowing` | `-Wfloat-conversion` | `score_communication_strict_warnings` | MISRA Rule 7.0.5, CWE-681 |
| `variable_shadowing` | `-Wshadow` | `score_communication_strict_warnings` | MISRA Rule 6.4.1, CWE-398 |
| `signed_unsigned_compare` | `-Wsign-compare` | `score_communication_strict_warnings` | CWE-195, CWE-697 |
| `unused_parameter` | `-Wunused-parameter` | `score_communication_strict_warnings` | MISRA Rule 0.1.1, CWE-561 |
| `delete_polymorphic` | `-Wdelete-non-virtual-dtor` | `score_communication_strict_warnings` (C++) | MISRA Rule 15.7.1 |
| `VDerived::process` | `-Woverloaded-virtual` | `score_communication_strict_warnings` (C++) | MISRA Rule 10.2.0 |
| `cast_away_const` | `-Wcast-qual` | `score_communication_additional_warnings` | MISRA Rule 8.2.3 |
| `format_nonliteral` | `-Wformat-nonliteral` | `score_communication_additional_warnings` | CWE-134 |
| `#if UNDEFINED_MACRO` | `-Wundef` | `score_communication_additional_warnings` | MISRA Dir 4.4 |

### False Positives (`clean.cpp`)

Demonstrates correct coding patterns that avoid triggering the above warnings:
`static_cast<>`, guarded comparisons, `(void)` cast, `[[maybe_unused]]`,
distinct variable names, const-safe access, literal format strings, defined macros,
virtual destructors, `using` declarations, and Clang thread-safety annotations.
