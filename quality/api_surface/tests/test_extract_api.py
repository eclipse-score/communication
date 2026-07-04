#!/usr/bin/env python3
# *******************************************************************************
# Copyright (c) 2026 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************

"""Tests for extract_api.py covering scenarios inspired by astgard test suite.

Test scenarios:
  1. Basic class: public methods extracted, private members excluded
  2. Type alias: follows 'using Foo = impl::Foo' to extract underlying members
  3. Inheritance: inherited public members are part of the public API
  4. Template alias: template type alias correctly extracts members
  5. Private changes: only public API tracked, private members ignored
  6. Types in signatures: struct members and method signatures captured
  7. Enum and free functions: enum values and free functions extracted
  8. Internal namespace filtering: detail::/internal:: namespaces excluded
"""

import json
import os
import subprocess
import sys
import tempfile
import unittest


def get_clang_binary():
    """Find clang++ binary for testing."""
    # Check common locations
    for candidate in [
        os.environ.get("CLANG_PATH", ""),
        "/usr/bin/clang++-19",
        "/usr/bin/clang++-18",
        "/usr/bin/clang++-17",
        "/usr/bin/clang++",
        "clang++",
    ]:
        if candidate and os.path.isfile(candidate):
            return candidate
        # Try to find in PATH
        if candidate and not os.path.sep in candidate:
            import shutil
            found = shutil.which(candidate)
            if found:
                return found
    return None


def run_extract_api(headers: list[str], target_files: list[str]) -> dict:
    """Run extract_api.py on the given headers and return parsed JSON output."""
    clang = get_clang_binary()
    if not clang:
        raise unittest.SkipTest("clang++ not found")

    test_dir = os.path.dirname(os.path.abspath(__file__))
    extract_script = os.path.join(os.path.dirname(test_dir), "extract_api.py")
    workspace_root = os.path.dirname(os.path.dirname(test_dir))

    # Run clang AST dump
    with tempfile.NamedTemporaryFile(
        mode="w", suffix=".cpp", delete=False, prefix="test_api_"
    ) as f:
        for header in headers:
            f.write(f'#include "{os.path.abspath(header)}"\n')
        combined_path = f.name

    try:
        cmd = [
            clang,
            "-Xclang", "-ast-dump=json",
            "-fsyntax-only",
            "-x", "c++",
            "-std=c++17",
            "-fparse-all-comments",
            "-w",
            "-I", workspace_root,
            combined_path,
        ]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        if not result.stdout:
            raise RuntimeError(f"clang produced no output. stderr: {result.stderr[:500]}")
        ast = json.loads(result.stdout)
    finally:
        os.unlink(combined_path)

    # Run extraction
    cmd = [
        sys.executable, extract_script,
        "--ast-json", "/dev/stdin",
        "--target-headers", ",".join(target_files),
        "--target-label", "//test:target",
    ]
    result = subprocess.run(
        cmd, input=json.dumps(ast), capture_output=True, text=True, timeout=30
    )
    if result.returncode != 0:
        raise RuntimeError(f"extract_api.py failed: {result.stderr}")
    return json.loads(result.stdout)


def get_test_header(name: str) -> str:
    """Get absolute path to a test header file."""
    test_dir = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(test_dir, name)


def get_symbols(result: dict) -> list[dict]:
    """Get all symbols (documented + undocumented) from result."""
    return result.get("symbols", []) + result.get("undocumented_symbols", [])


def symbol_names(symbols: list[dict]) -> set[str]:
    """Extract just the names from a list of symbols."""
    return {s["name"] for s in symbols}


def qualified_names(symbols: list[dict]) -> set[str]:
    """Extract qualified names from a list of symbols."""
    return {s["qualified_name"] for s in symbols}


def find_symbol(symbols: list[dict], qualified_name: str) -> dict | None:
    """Find a symbol by qualified name."""
    for s in symbols:
        if s["qualified_name"] == qualified_name:
            return s
    return None


class TestBasicClass(unittest.TestCase):
    """Test case 1: Basic class extraction (matches astgard test_original.cpp)."""

    @classmethod
    def setUpClass(cls):
        header = get_test_header("basic_class.h")
        cls.result = run_extract_api([header], [header])
        cls.symbols = get_symbols(cls.result)

    def test_class_extracted(self):
        """The class itself is in the API surface."""
        self.assertIn("test::Calculator", qualified_names(self.symbols))

    def test_public_methods_extracted(self):
        """All public methods are extracted."""
        names = symbol_names(self.symbols)
        self.assertIn("add", names)
        self.assertIn("subtract", names)
        self.assertIn("multiply", names)
        self.assertIn("reset", names)

    def test_constructor_destructor_extracted(self):
        """Constructor and destructor are extracted."""
        names = symbol_names(self.symbols)
        self.assertIn("Calculator", names)  # constructor
        self.assertIn("~Calculator", names)  # destructor

    def test_private_members_excluded(self):
        """Private members are NOT in the API surface."""
        names = symbol_names(self.symbols)
        self.assertNotIn("result_", names)
        self.assertNotIn("initialized_", names)

    def test_method_signatures(self):
        """Method signatures contain return type and parameters."""
        sym = find_symbol(self.symbols, "test::Calculator::add")
        self.assertIsNotNone(sym)
        self.assertIn("int", sym["signature"])

    def test_method_kind(self):
        """Methods have correct kind."""
        sym = find_symbol(self.symbols, "test::Calculator::add")
        self.assertIsNotNone(sym)
        self.assertIn(sym["kind"], ("method",))


class TestTypeAlias(unittest.TestCase):
    """Test case 2: Type alias following (using Foo = impl::Foo)."""

    @classmethod
    def setUpClass(cls):
        header = get_test_header("type_alias.h")
        cls.result = run_extract_api([header], [header])
        cls.symbols = get_symbols(cls.result)

    def test_alias_itself_extracted(self):
        """The type alias declaration appears in the API."""
        self.assertIn("test::Point", qualified_names(self.symbols))

    def test_underlying_methods_extracted(self):
        """Public methods from the underlying impl class are exposed."""
        names = symbol_names(self.symbols)
        self.assertIn("x", names)
        self.assertIn("y", names)
        self.assertIn("distance", names)

    def test_members_qualified_under_alias(self):
        """Extracted members use the alias qualified name, not impl."""
        qnames = qualified_names(self.symbols)
        self.assertIn("test::Point::x", qnames)
        self.assertIn("test::Point::y", qnames)
        self.assertIn("test::Point::distance", qnames)
        # Should NOT appear under impl namespace
        self.assertNotIn("test::impl::PointImpl::x", qnames)

    def test_private_impl_members_excluded(self):
        """Private members from impl class are not exposed."""
        names = symbol_names(self.symbols)
        self.assertNotIn("x_", names)
        self.assertNotIn("y_", names)


class TestInheritance(unittest.TestCase):
    """Test case 3: Inheritance (matches astgard indirect type concept)."""

    @classmethod
    def setUpClass(cls):
        header = get_test_header("inheritance.h")
        cls.result = run_extract_api([header], [header])
        cls.symbols = get_symbols(cls.result)

    def test_alias_extracted(self):
        """The Circle type alias is in the API."""
        self.assertIn("test::Circle", qualified_names(self.symbols))

    def test_own_methods_extracted(self):
        """Circle's own public methods are extracted."""
        qnames = qualified_names(self.symbols)
        self.assertIn("test::Circle::Circle", qnames)  # constructor
        self.assertIn("test::Circle::radius", qnames)

    def test_inherited_methods_extracted(self):
        """Public methods from base class Shape are also extracted."""
        qnames = qualified_names(self.symbols)
        self.assertIn("test::Circle::area", qnames)
        self.assertIn("test::Circle::perimeter", qnames)

    def test_protected_base_members_excluded(self):
        """Protected members from base class are NOT in public API."""
        names = symbol_names(self.symbols)
        self.assertNotIn("setDirty", names)

    def test_private_base_members_excluded(self):
        """Private members from base class are NOT in public API."""
        names = symbol_names(self.symbols)
        self.assertNotIn("cached_area_", names)


class TestTemplateAlias(unittest.TestCase):
    """Test case 4: Template type alias."""

    @classmethod
    def setUpClass(cls):
        header = get_test_header("template_alias.h")
        cls.result = run_extract_api([header], [header])
        cls.symbols = get_symbols(cls.result)

    def test_template_alias_extracted(self):
        """The template type alias is in the API surface."""
        self.assertIn("test::Container", qualified_names(self.symbols))

    def test_template_members_extracted(self):
        """Public methods from the underlying template class are extracted."""
        names = symbol_names(self.symbols)
        self.assertIn("push", names)
        self.assertIn("pop", names)
        self.assertIn("size", names)
        self.assertIn("empty", names)

    def test_private_template_members_excluded(self):
        """Private members from template class are excluded."""
        names = symbol_names(self.symbols)
        self.assertNotIn("data_", names)
        self.assertNotIn("size_", names)


class TestPrivateChanges(unittest.TestCase):
    """Test case 5: Private changes don't affect public API (astgard test_private_changes)."""

    @classmethod
    def setUpClass(cls):
        header = get_test_header("private_changes.h")
        cls.result = run_extract_api([header], [header])
        cls.symbols = get_symbols(cls.result)

    def test_public_methods_only(self):
        """Only public methods appear in the API."""
        names = symbol_names(self.symbols)
        self.assertIn("StableApi", names)  # constructor
        self.assertIn("process", names)
        self.assertIn("name", names)

    def test_private_members_absent(self):
        """Private members and methods are not in API."""
        names = symbol_names(self.symbols)
        self.assertNotIn("name_", names)
        self.assertNotIn("history_", names)
        self.assertNotIn("cache_", names)
        self.assertNotIn("updateHistory", names)
        self.assertNotIn("clearCache", names)


class TestTypesInSignatures(unittest.TestCase):
    """Test case 6: Types used in signatures (astgard test_types_*)."""

    @classmethod
    def setUpClass(cls):
        header = get_test_header("types_in_signatures.h")
        cls.result = run_extract_api([header], [header])
        cls.symbols = get_symbols(cls.result)

    def test_struct_public_members(self):
        """Public members of structs are extracted."""
        qnames = qualified_names(self.symbols)
        self.assertIn("test::Point", qnames)
        self.assertIn("test::Point::x", qnames)
        self.assertIn("test::Point::y", qnames)
        self.assertIn("test::Point::distance", qnames)

    def test_signature_captures_types(self):
        """Method signatures include the types used."""
        sym = find_symbol(self.symbols, "test::GeometryCalculator::calculateMidpoint")
        self.assertIsNotNone(sym)
        self.assertIn("Point", sym["signature"])

    def test_indirect_type_in_api(self):
        """Line struct (which uses Point) is also part of the API."""
        qnames = qualified_names(self.symbols)
        self.assertIn("test::Line", qnames)
        self.assertIn("test::Line::length", qnames)

    def test_all_classes_extracted(self):
        """All public structs/classes are in the API."""
        qnames = qualified_names(self.symbols)
        self.assertIn("test::Point", qnames)
        self.assertIn("test::Line", qnames)
        self.assertIn("test::GeometryCalculator", qnames)


class TestEnumAndFreeFunctions(unittest.TestCase):
    """Test case 7: Enums and free functions."""

    @classmethod
    def setUpClass(cls):
        header = get_test_header("enum_and_free_functions.h")
        cls.result = run_extract_api([header], [header])
        cls.symbols = get_symbols(cls.result)

    def test_enum_extracted(self):
        """Scoped enum is in the API."""
        self.assertIn("test::Status", qualified_names(self.symbols))

    def test_enum_values_extracted(self):
        """Enum values are extracted."""
        qnames = qualified_names(self.symbols)
        self.assertIn("test::Status::kOk", qnames)
        self.assertIn("test::Status::kError", qnames)
        self.assertIn("test::Status::kTimeout", qnames)
        self.assertIn("test::Status::kCancelled", qnames)

    def test_free_functions_extracted(self):
        """Free (namespace-level) functions are extracted."""
        qnames = qualified_names(self.symbols)
        self.assertIn("test::Initialize", qnames)
        self.assertIn("test::Shutdown", qnames)
        self.assertIn("test::ProcessData", qnames)

    def test_function_signature(self):
        """Free function signature includes return type and params."""
        sym = find_symbol(self.symbols, "test::ProcessData")
        self.assertIsNotNone(sym)
        self.assertIn("int", sym["signature"])
        self.assertIn("char", sym["signature"])


class TestInternalNamespaceFiltering(unittest.TestCase):
    """Test case 8: Internal/detail namespaces are excluded."""

    @classmethod
    def setUpClass(cls):
        header = get_test_header("internal_namespace.h")
        cls.result = run_extract_api([header], [header])
        cls.symbols = get_symbols(cls.result)

    def test_public_class_extracted(self):
        """The public class is in the API."""
        qnames = qualified_names(self.symbols)
        self.assertIn("test::PublicService", qnames)
        self.assertIn("test::PublicService::start", qnames)
        self.assertIn("test::PublicService::stop", qnames)

    def test_detail_namespace_excluded(self):
        """Classes in detail:: namespace are NOT in the API."""
        qnames = qualified_names(self.symbols)
        self.assertNotIn("test::detail::InternalHelper", qnames)
        self.assertNotIn("test::detail::InternalHelper::doStuff", qnames)
        self.assertNotIn("test::detail::InternalHelper::compute", qnames)

    def test_internal_namespace_excluded(self):
        """Classes in internal:: namespace are NOT in the API."""
        qnames = qualified_names(self.symbols)
        self.assertNotIn("test::internal::AnotherInternal", qnames)
        self.assertNotIn("test::internal::AnotherInternal::internalMethod", qnames)


class TestLockFileFormat(unittest.TestCase):
    """Test that the lock file output format is correct (no file/line metadata)."""

    @classmethod
    def setUpClass(cls):
        header = get_test_header("basic_class.h")
        cls.result = run_extract_api([header], [header])

    def test_has_required_fields(self):
        """Symbols have name, qualified_name, kind, signature."""
        for sym in self.result.get("symbols", []):
            self.assertIn("name", sym)
            self.assertIn("qualified_name", sym)
            self.assertIn("kind", sym)
            self.assertIn("signature", sym)

    def test_no_file_metadata(self):
        """Symbols do NOT have file/line/doc metadata."""
        for sym in self.result.get("symbols", []) + self.result.get("undocumented_symbols", []):
            self.assertNotIn("file", sym)
            self.assertNotIn("line", sym)
            self.assertNotIn("has_api_marker", sym)
            self.assertNotIn("has_brief_doc", sym)

    def test_has_version(self):
        """Output has a version field."""
        self.assertIn("version", self.result)

    def test_documented_vs_undocumented_split(self):
        """Symbols with \\api marker go to 'symbols', others to 'undocumented_symbols'."""
        self.assertIn("symbols", self.result)
        self.assertIn("undocumented_symbols", self.result)


if __name__ == "__main__":
    unittest.main()
