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

"""Validation tests for extract_api.py covering scenarios from astgard test suite.

These tests run as a Bazel py_test that reads the generated API surface JSON files
(produced by the api_surface_gen rule using clang from the LLVM toolchain).

Test scenarios (inspired by an internal astgard test suite):
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
import unittest


def load_api_surface(name: str) -> dict:
    """Load generated API surface JSON from Bazel runfiles."""
    # Bazel runfiles path
    runfiles_dir = os.environ.get("TEST_SRCDIR", "")
    workspace_name = os.environ.get("TEST_WORKSPACE", "")

    # Try several path patterns
    candidates = [
        os.path.join(runfiles_dir, workspace_name, "quality/api_surface/tests", f"{name}_api_test_gen.json"),
        os.path.join(runfiles_dir, workspace_name, f"quality/api_surface/tests/{name}_api_test_gen.json"),
        f"quality/api_surface/tests/{name}_api_test_gen.json",
    ]

    for path in candidates:
        if os.path.isfile(path):
            with open(path) as f:
                return json.load(f)

    # Fallback: search runfiles
    if runfiles_dir:
        for root, dirs, files in os.walk(runfiles_dir):
            for fname in files:
                if fname == f"{name}_api_test_gen.json":
                    with open(os.path.join(root, fname)) as f:
                        return json.load(f)

    raise FileNotFoundError(
        f"Could not find {name}_api_test_gen.json in runfiles. "
        f"Searched: {candidates}"
    )


def get_all_symbols(result: dict) -> list[dict]:
    """Get all symbols (documented + undocumented) from result."""
    return result.get("symbols", []) + result.get("undocumented_symbols", [])


def symbol_names(symbols: list[dict]) -> set[str]:
    """Extract names from symbols."""
    return {s["name"] for s in symbols}


def qualified_names(symbols: list[dict]) -> set[str]:
    """Extract qualified names from symbols."""
    return {s["qualified_name"] for s in symbols}


def find_symbol(symbols: list[dict], qualified_name: str) -> dict | None:
    """Find a symbol by qualified name."""
    for s in symbols:
        if s["qualified_name"] == qualified_name:
            return s
    return None


class TestBasicClass(unittest.TestCase):
    """Basic class: public methods extracted, private members excluded."""

    @classmethod
    def setUpClass(cls):
        cls.result = load_api_surface("basic_class")
        cls.symbols = get_all_symbols(cls.result)

    def test_class_extracted(self):
        self.assertIn("test::Calculator", qualified_names(self.symbols))

    def test_public_methods_extracted(self):
        names = symbol_names(self.symbols)
        for method in ("add", "subtract", "multiply", "reset"):
            self.assertIn(method, names, f"Missing public method: {method}")

    def test_constructor_destructor_extracted(self):
        names = symbol_names(self.symbols)
        self.assertIn("Calculator", names)
        self.assertIn("~Calculator", names)

    def test_private_members_excluded(self):
        names = symbol_names(self.symbols)
        self.assertNotIn("result_", names)
        self.assertNotIn("initialized_", names)

    def test_method_signature_has_types(self):
        sym = find_symbol(self.symbols, "test::Calculator::add")
        self.assertIsNotNone(sym)
        self.assertIn("int", sym["signature"])

    def test_symbol_kind(self):
        sym = find_symbol(self.symbols, "test::Calculator::add")
        self.assertEqual(sym["kind"], "method")
        sym = find_symbol(self.symbols, "test::Calculator")
        self.assertEqual(sym["kind"], "class")


class TestTypeAlias(unittest.TestCase):
    """Type alias: follows 'using Point = impl::PointImpl' to expose members."""

    @classmethod
    def setUpClass(cls):
        cls.result = load_api_surface("type_alias")
        cls.symbols = get_all_symbols(cls.result)

    def test_alias_extracted(self):
        self.assertIn("test::Point", qualified_names(self.symbols))

    def test_underlying_methods_exposed(self):
        qnames = qualified_names(self.symbols)
        self.assertIn("test::Point::x", qnames)
        self.assertIn("test::Point::y", qnames)
        self.assertIn("test::Point::distance", qnames)

    def test_impl_namespace_not_exposed(self):
        """Members appear under the alias name, not the impl name."""
        qnames = qualified_names(self.symbols)
        for qn in qnames:
            self.assertNotIn("impl::PointImpl", qn)

    def test_private_impl_members_excluded(self):
        names = symbol_names(self.symbols)
        self.assertNotIn("x_", names)
        self.assertNotIn("y_", names)


class TestInheritance(unittest.TestCase):
    """Inheritance: inherited public members from base class are in the API."""

    @classmethod
    def setUpClass(cls):
        cls.result = load_api_surface("inheritance")
        cls.symbols = get_all_symbols(cls.result)

    def test_alias_extracted(self):
        self.assertIn("test::Circle", qualified_names(self.symbols))

    def test_own_methods(self):
        qnames = qualified_names(self.symbols)
        self.assertIn("test::Circle::Circle", qnames)
        self.assertIn("test::Circle::radius", qnames)

    def test_inherited_public_methods(self):
        """Public methods from Shape base class should appear on Circle."""
        qnames = qualified_names(self.symbols)
        self.assertIn("test::Circle::area", qnames)
        self.assertIn("test::Circle::perimeter", qnames)

    def test_protected_base_excluded(self):
        names = symbol_names(self.symbols)
        self.assertNotIn("setDirty", names)

    def test_private_base_excluded(self):
        names = symbol_names(self.symbols)
        self.assertNotIn("cached_area_", names)


class TestTemplateAlias(unittest.TestCase):
    """Template alias: 'template<T> using Container = impl::ContainerImpl<T>' works."""

    @classmethod
    def setUpClass(cls):
        cls.result = load_api_surface("template_alias")
        cls.symbols = get_all_symbols(cls.result)

    def test_template_alias_extracted(self):
        self.assertIn("test::Container", qualified_names(self.symbols))

    def test_template_members_exposed(self):
        names = symbol_names(self.symbols)
        for member in ("push", "pop", "size", "empty"):
            self.assertIn(member, names, f"Missing template member: {member}")

    def test_private_template_members_excluded(self):
        names = symbol_names(self.symbols)
        self.assertNotIn("data_", names)
        self.assertNotIn("size_", names)

    def test_alias_kind(self):
        sym = find_symbol(self.symbols, "test::Container")
        self.assertEqual(sym["kind"], "template_type_alias")


class TestPrivateChanges(unittest.TestCase):
    """Private changes: private members and methods are not part of API."""

    @classmethod
    def setUpClass(cls):
        cls.result = load_api_surface("private_changes")
        cls.symbols = get_all_symbols(cls.result)

    def test_public_methods_present(self):
        names = symbol_names(self.symbols)
        self.assertIn("process", names)
        self.assertIn("name", names)

    def test_private_members_absent(self):
        names = symbol_names(self.symbols)
        for private in ("name_", "history_", "cache_", "updateHistory", "clearCache"):
            self.assertNotIn(private, names, f"Private member leaked: {private}")


class TestTypesInSignatures(unittest.TestCase):
    """Types in signatures: structs and their members in the API."""

    @classmethod
    def setUpClass(cls):
        cls.result = load_api_surface("types_in_signatures")
        cls.symbols = get_all_symbols(cls.result)

    def test_struct_extracted(self):
        qnames = qualified_names(self.symbols)
        self.assertIn("test::Point", qnames)
        self.assertIn("test::Line", qnames)

    def test_struct_public_members(self):
        qnames = qualified_names(self.symbols)
        self.assertIn("test::Point::x", qnames)
        self.assertIn("test::Point::y", qnames)
        self.assertIn("test::Point::distance", qnames)

    def test_signature_captures_param_types(self):
        """Method signatures include types used as parameters."""
        sym = find_symbol(self.symbols, "test::GeometryCalculator::calculateMidpoint")
        self.assertIsNotNone(sym)
        self.assertIn("Point", sym["signature"])

    def test_indirect_type_tracked(self):
        """Line (which uses Point) is tracked in the API surface."""
        qnames = qualified_names(self.symbols)
        self.assertIn("test::Line", qnames)
        self.assertIn("test::Line::length", qnames)

    def test_class_methods(self):
        qnames = qualified_names(self.symbols)
        self.assertIn("test::GeometryCalculator::calculateDistance", qnames)
        self.assertIn("test::GeometryCalculator::minLineDistance", qnames)


class TestEnumAndFreeFunctions(unittest.TestCase):
    """Enum and free functions: scoped enums and namespace-level functions."""

    @classmethod
    def setUpClass(cls):
        cls.result = load_api_surface("enum_and_free_functions")
        cls.symbols = get_all_symbols(cls.result)

    def test_enum_extracted(self):
        self.assertIn("test::Status", qualified_names(self.symbols))

    def test_enum_values(self):
        qnames = qualified_names(self.symbols)
        for val in ("test::Status::kOk", "test::Status::kError",
                    "test::Status::kTimeout", "test::Status::kCancelled"):
            self.assertIn(val, qnames, f"Missing enum value: {val}")

    def test_free_functions(self):
        qnames = qualified_names(self.symbols)
        self.assertIn("test::Initialize", qnames)
        self.assertIn("test::Shutdown", qnames)
        self.assertIn("test::ProcessData", qnames)

    def test_function_kind(self):
        sym = find_symbol(self.symbols, "test::Initialize")
        self.assertEqual(sym["kind"], "function")

    def test_function_signature_types(self):
        sym = find_symbol(self.symbols, "test::ProcessData")
        self.assertIsNotNone(sym)
        self.assertIn("int", sym["signature"])


class TestInternalNamespaceFiltering(unittest.TestCase):
    """Internal namespace filtering: detail::/internal:: excluded."""

    @classmethod
    def setUpClass(cls):
        cls.result = load_api_surface("internal_namespace")
        cls.symbols = get_all_symbols(cls.result)

    def test_public_class_present(self):
        qnames = qualified_names(self.symbols)
        self.assertIn("test::PublicService", qnames)
        self.assertIn("test::PublicService::start", qnames)
        self.assertIn("test::PublicService::stop", qnames)

    def test_detail_excluded(self):
        qnames = qualified_names(self.symbols)
        for qn in qnames:
            self.assertNotIn("detail::", qn, f"Internal symbol leaked: {qn}")

    def test_internal_excluded(self):
        qnames = qualified_names(self.symbols)
        for qn in qnames:
            self.assertNotIn("internal::", qn, f"Internal symbol leaked: {qn}")


class TestLockFileFormat(unittest.TestCase):
    """Lock file format: correct structure, no file/line metadata."""

    @classmethod
    def setUpClass(cls):
        cls.result = load_api_surface("basic_class")

    def test_required_top_level_keys(self):
        self.assertIn("target", self.result)
        self.assertIn("symbols", self.result)
        self.assertIn("undocumented_symbols", self.result)
        self.assertIn("version", self.result)

    def test_symbol_has_only_api_shape_fields(self):
        """Symbols contain only name, qualified_name, kind, signature."""
        allowed = {"name", "qualified_name", "kind", "signature"}
        for sym in get_all_symbols(self.result):
            self.assertEqual(set(sym.keys()), allowed,
                           f"Unexpected fields in symbol: {set(sym.keys()) - allowed}")

    def test_no_file_metadata(self):
        for sym in get_all_symbols(self.result):
            self.assertNotIn("file", sym)
            self.assertNotIn("line", sym)
            self.assertNotIn("has_api_marker", sym)
            self.assertNotIn("has_brief_doc", sym)


class TestResultTypeRegression(unittest.TestCase):
    """Regression: result type signatures resolve from transitive public headers."""

    @classmethod
    def setUpClass(cls):
        cls.result = load_api_surface("result_type_regression")
        cls.symbols = get_all_symbols(cls.result)

    def test_known_result_signatures_are_not_degraded_to_int(self):
        symbol = find_symbol(self.symbols, "test::ResultFactory::CreateWidget")
        self.assertIsNotNone(symbol, "Missing symbol: test::ResultFactory::CreateWidget")
        signature = symbol["signature"]
        self.assertIn("support::Result<support::Widget>", signature)
        self.assertNotIn(" : int ", signature)

    def test_transitive_public_types_are_resolved(self):
        qnames = qualified_names(self.symbols)
        self.assertIn("test::support::Result", qnames)
        self.assertIn("test::support::Widget", qnames)

    def test_lock_file_scope_stays_reasonable(self):
        self.assertLess(len(self.symbols), 50)


if __name__ == "__main__":
    unittest.main()
