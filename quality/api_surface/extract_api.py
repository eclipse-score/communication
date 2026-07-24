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

"""Extract public C++ API surface from headers using clang's AST dump.

Uses `clang -Xclang -ast-dump=json` for robust C++ parsing that correctly
handles preprocessor directives, macros, templates, and conditional compilation.

Modes:
  1. --ast-json: Parse a pre-generated clang AST JSON dump (used by Bazel action)
"""

import argparse
import json
import os
import re
import sys
from dataclasses import asdict, dataclass, field
from typing import Optional


@dataclass
class ApiSymbol:
    """Represents a single public API symbol."""

    name: str
    qualified_name: str
    kind: str
    signature: str
    file: str
    line: int
    has_api_marker: bool
    has_brief_doc: bool


@dataclass
class ApiSurface:
    """The complete API surface of a library."""

    target: str
    symbols: list
    undocumented_symbols: list = field(default_factory=list)
    version: str = "1"


# Namespace patterns considered internal (not public API)
INTERNAL_NAMESPACE_PATTERNS = ("::detail::", "::detail", "::internal::", "::internal", "::impl::")


def is_internal_name(qualified_name: str) -> bool:
    """Check if a symbol is in an internal/detail namespace."""
    for pattern in INTERNAL_NAMESPACE_PATTERNS:
        if pattern in qualified_name:
            return True
    return False


def get_comment_markers(comment_node: Optional[dict]) -> tuple[bool, bool]:
    """Recursively check a FullComment AST node for \\api and \\brief markers."""
    if not comment_node:
        return False, False

    has_api = False
    has_brief = False

    def walk_comment(node):
        nonlocal has_api, has_brief
        kind = node.get("kind", "")

        if kind == "BlockCommandComment":
            cmd = node.get("name", "")
            if cmd == "api":
                has_api = True
            elif cmd == "brief":
                has_brief = True

        if kind == "InlineCommandComment":
            cmd = node.get("name", "")
            if cmd == "api":
                has_api = True
            elif cmd == "brief":
                has_brief = True

        if kind == "TextComment":
            text = node.get("text", "")
            if re.search(r"[\\@]api\b", text):
                has_api = True
            if re.search(r"[\\@]brief\b", text):
                has_brief = True

        for child in node.get("inner", []):
            walk_comment(child)

    walk_comment(comment_node)
    return has_api, has_brief


def build_qualified_name(name: str, namespace_stack: list[str]) -> str:
    """Build fully qualified name from namespace stack."""
    prefix = "::".join(namespace_stack)
    return f"{prefix}::{name}" if prefix else name


def extract_signature(node: dict) -> str:
    """Extract a human-readable signature from an AST node."""
    kind = node.get("kind", "")
    name = node.get("name", "")
    type_info = node.get("type", {}).get("qualType", "")

    if kind in ("FunctionDecl", "CXXMethodDecl", "FunctionTemplateDecl",
                "CXXConstructorDecl", "CXXDestructorDecl"):
        return f"{name} : {type_info}" if type_info else name
    elif kind in ("TypeAliasDecl", "TypedefDecl", "TypeAliasTemplateDecl"):
        return f"using {name} = {type_info}" if type_info else f"using {name}"
    elif kind == "CXXRecordDecl":
        tag = node.get("tagUsed", "class")
        return f"{tag} {name}"
    elif kind == "EnumDecl":
        scoped = node.get("scopedEnumTag", "")
        return f"enum {scoped} {name}" if scoped else f"enum {name}"
    elif kind == "EnumConstantDecl":
        return name
    elif kind == "VarDecl":
        return f"{name} : {type_info}" if type_info else name
    return name


# Map clang attribute node kinds to their C++ standard attribute spelling.
ATTR_SPELLINGS = {
    "WarnUnusedResultAttr": "nodiscard",
    "DeprecatedAttr": "deprecated",
    "NoReturnAttr": "noreturn",
    "CXX11NoReturnAttr": "noreturn",
    "UnusedAttr": "maybe_unused",
    "CarriesDependencyAttr": "carries_dependency",
    "LikelyAttr": "likely",
    "UnlikelyAttr": "unlikely",
}


def extract_attributes(node: dict) -> list[str]:
    """Collect C++ standard attribute spellings ([[...]]) from a decl node.

    Only explicit (non-implicit) attributes with a known standard spelling are
    reported, so that adding/removing/changing e.g. [[nodiscard]] or
    [[deprecated]] is detected as an API change while implicit compiler
    attributes are ignored.
    """
    attrs: list[str] = []
    for child in node.get("inner", []):
        kind = child.get("kind", "")
        if not kind.endswith("Attr"):
            continue
        if child.get("implicit"):
            continue
        spelling = ATTR_SPELLINGS.get(kind)
        if spelling:
            attrs.append(f"[[{spelling}]]")
    return attrs


def enrich_signature(base_signature: str, node: dict, extern_c: bool = False) -> str:
    """Add API-relevant qualifiers to a signature.

    Captures properties that are part of the public contract but are not part of
    the plain clang qualType: ``constexpr``, ``extern`` storage, ``extern "C"``
    linkage, and C++ standard attributes ([[nodiscard]], [[deprecated]], ...).
    """
    prefixes: list[str] = []
    if extern_c:
        prefixes.append('extern "C"')
    if node.get("storageClass") == "extern":
        prefixes.append("extern")
    if node.get("storageClass") == "static":
        prefixes.append("static")
    if node.get("constexpr"):
        prefixes.append("constexpr")

    result = base_signature
    if prefixes:
        result = " ".join(prefixes) + " " + result

    attrs = extract_attributes(node)
    if attrs:
        result = result + " " + " ".join(attrs)
    return result


def map_kind(node: dict) -> str:
    """Map AST node kind to our simplified kind string."""
    kind = node.get("kind", "")
    mapping = {
        "FunctionDecl": "function",
        "CXXMethodDecl": "method",
        "CXXConstructorDecl": "constructor",
        "CXXDestructorDecl": "destructor",
        "TypeAliasDecl": "type_alias",
        "TypedefDecl": "type_alias",
        "TypeAliasTemplateDecl": "template_type_alias",
        "FunctionTemplateDecl": "template_function",
        "ClassTemplateDecl": "template_class",
        "EnumDecl": "enum",
        "EnumConstantDecl": "enum_value",
        "VarDecl": "variable",
        "FieldDecl": "field",
    }
    if kind == "CXXRecordDecl":
        tag = node.get("tagUsed", "class")
        return "struct" if tag == "struct" else "class"
    return mapping.get(kind, kind)


def is_in_target_files(file_path: str, target_files: set[str]) -> bool:
    """Check if a file path matches one of our target header files."""
    if not file_path:
        return False
    # Normalize: remove leading ./ and normalize path
    normalized = os.path.normpath(file_path)
    for target in target_files:
        norm_target = os.path.normpath(target)
        if normalized == norm_target:
            return True
        if normalized.endswith("/" + norm_target) or normalized.endswith(os.sep + norm_target):
            return True
    return False


def find_comment(node: dict) -> Optional[dict]:
    """Find FullComment node in a declaration's inner nodes."""
    for child in node.get("inner", []):
        if child.get("kind") == "FullComment":
            return child
    return None


def _get_underlying_type_for_alias(ast_root: dict, name: str, qualified_name: str) -> Optional[str]:
    """Find the underlying type (qualType) for a type alias in the AST."""

    def search(node: dict, ns_stack: list[str]) -> Optional[str]:
        kind = node.get("kind", "")
        node_name = node.get("name", "")

        if kind == "NamespaceDecl":
            new_stack = ns_stack + [node_name] if node_name else ns_stack
            for child in node.get("inner", []):
                result = search(child, new_stack)
                if result is not None:
                    return result
            return None

        if kind in ("TypeAliasDecl", "TypedefDecl") and node_name == name:
            node_qualified = build_qualified_name(node_name, ns_stack)
            if node_qualified == qualified_name:
                return node.get("type", {}).get("qualType", "")

        if kind == "TypeAliasTemplateDecl" and node_name == name:
            node_qualified = build_qualified_name(node_name, ns_stack)
            if node_qualified == qualified_name:
                # Get type from inner TypeAliasDecl
                for child in node.get("inner", []):
                    if child.get("kind") == "TypeAliasDecl":
                        return child.get("type", {}).get("qualType", "")

        # Recurse (including into classes for nested types)
        for child in node.get("inner", []):
            result = search(child, ns_stack)
            if result is not None:
                return result
        return None

    for node in ast_root.get("inner", []):
        result = search(node, [])
        if result is not None:
            return result
    return None


def build_file_map(ast_root: dict) -> dict[str, str]:
    """Pre-compute file path for every node in the AST.

    Clang's JSON AST uses 'sticky' file paths - the 'file' field only appears
    when the current file changes. We do a DFS to propagate the file path
    to all nodes.
    """
    file_map: dict[str, str] = {}

    def dfs(node: dict, current_file: str) -> str:
        node_id = node.get("id", "")
        loc = node.get("loc", {})

        # Resolve file from loc (handling various formats)
        file_in_loc = ""
        if "expansionLoc" in loc:
            file_in_loc = loc["expansionLoc"].get("file", "")
        elif "spellingLoc" in loc:
            file_in_loc = loc["spellingLoc"].get("file", "")
        else:
            file_in_loc = loc.get("file", "")

        if file_in_loc:
            current_file = file_in_loc

        if node_id:
            file_map[node_id] = current_file

        # Also check range.begin for file changes
        range_info = node.get("range", {})
        begin = range_info.get("begin", {})
        if "file" in begin:
            current_file = begin["file"]
            if node_id:
                file_map[node_id] = current_file

        for child in node.get("inner", []):
            current_file = dfs(child, current_file)

        return current_file

    dfs(ast_root, "")
    return file_map


def extract_from_ast(
    ast_root: dict,
    target_files: set[str],
    target_label: str,
) -> ApiSurface:
    """Walk the clang AST and extract public API symbols.

    Uses a pre-built file map to handle clang's sticky file tracking.
    """
    symbols: list[ApiSymbol] = []

    # Pre-compute file ownership for all nodes
    file_map = build_file_map(ast_root)

    # Pre-compute the set of qualified names that have a complete class/struct
    # definition, so forward declarations of already-defined types are not
    # reported as separate incomplete-type symbols.
    defined_record_qnames: set[str] = set()

    def _collect_defined(node: dict, ns_stack: list[str]):
        kind = node.get("kind", "")
        name = node.get("name", "")
        if kind == "NamespaceDecl":
            new_stack = ns_stack + [name] if name else ns_stack
            for child in node.get("inner", []):
                _collect_defined(child, new_stack)
            return
        if kind in ("CXXRecordDecl", "ClassTemplateSpecializationDecl") and name and node.get("completeDefinition"):
            defined_record_qnames.add(build_qualified_name(name, ns_stack))
        for child in node.get("inner", []):
            _collect_defined(child, ns_stack)

    for _n in ast_root.get("inner", []):
        _collect_defined(_n, [])

    # Track emitted forward declarations to avoid duplicates.
    seen_forward: set[str] = set()

    def get_node_file(node: dict) -> str:
        return file_map.get(node.get("id", ""), "")

    def get_line(node: dict) -> int:
        loc = node.get("loc", {})
        if "expansionLoc" in loc:
            return loc["expansionLoc"].get("line", 0)
        if "spellingLoc" in loc:
            return loc["spellingLoc"].get("line", 0)
        return loc.get("line", 0)

    def make_relative(file_path: str) -> str:
        if not file_path:
            return ""
        for target in target_files:
            if file_path.endswith(target) or file_path == target:
                return target
        return file_path

    def walk(node: dict, namespace_stack: list[str], access: str = "public",
             extern_c: bool = False):
        kind = node.get("kind", "")
        name = node.get("name", "")
        effective_file = get_node_file(node)
        line = get_line(node)
        in_target = is_in_target_files(effective_file, target_files)

        # Handle namespaces
        if kind == "NamespaceDecl":
            ns_name = name if name else ""
            new_stack = namespace_stack + [ns_name] if ns_name else namespace_stack
            for child in node.get("inner", []):
                walk(child, new_stack, "public", extern_c)
            return

        # Handle extern "C" / extern "C++" linkage specifications.
        if kind == "LinkageSpecDecl":
            is_c = node.get("language") == "C"
            for child in node.get("inner", []):
                walk(child, namespace_stack, access, extern_c or is_c)
            return

        # Handle explicit (extern) template instantiations, e.g.
        # `extern template class Foo<int>;`.
        if kind == "ClassTemplateSpecializationDecl":
            if not in_target or not name or access not in ("public", "protected"):
                return
            args = []
            for child in node.get("inner", []):
                if child.get("kind") == "TemplateArgument":
                    args.append(child.get("type", {}).get("qualType", ""))
            arg_str = ", ".join(a for a in args if a)
            display = f"{name}<{arg_str}>"
            qualified = build_qualified_name(display, namespace_stack)
            if is_internal_name(qualified) or qualified in seen_forward:
                return
            seen_forward.add(qualified)
            tag = node.get("tagUsed", "class")
            kind_str = "extern_template" if access == "public" else "protected_extern_template"
            symbols.append(ApiSymbol(
                name=name, qualified_name=qualified, kind=kind_str,
                signature=f"extern template {tag} {display}",
                file=make_relative(effective_file), line=line,
                has_api_marker=False, has_brief_doc=False,
            ))
            return

        # Handle class/struct declarations
        if kind == "CXXRecordDecl":
            is_definition = node.get("completeDefinition", False)
            if node.get("isImplicit"):
                return

            # Forward-declared / incomplete type (e.g. pimpl `class Impl;`).
            if not is_definition:
                if in_target and name and access in ("public", "protected"):
                    qualified = build_qualified_name(name, namespace_stack)
                    if (not is_internal_name(qualified)
                            and qualified not in defined_record_qnames
                            and qualified not in seen_forward):
                        seen_forward.add(qualified)
                        fwd_kind = ("forward_class" if access == "public"
                                    else "protected_forward_class")
                        symbols.append(ApiSymbol(
                            name=name, qualified_name=qualified, kind=fwd_kind,
                            signature=extract_signature(node),
                            file=make_relative(effective_file), line=line,
                            has_api_marker=False, has_brief_doc=False,
                        ))
                return

            if in_target and name and access in ("public", "protected"):
                qualified = build_qualified_name(name, namespace_stack)
                if not is_internal_name(qualified):
                    comment = find_comment(node)
                    has_api, has_brief = get_comment_markers(comment)
                    rec_kind = map_kind(node)
                    if access == "protected":
                        rec_kind = "protected_" + rec_kind
                    symbols.append(ApiSymbol(
                        name=name,
                        qualified_name=qualified,
                        kind=rec_kind,
                        signature=extract_signature(node),
                        file=make_relative(effective_file),
                        line=line,
                        has_api_marker=has_api,
                        has_brief_doc=has_brief,
                    ))

            # Walk class members
            member_access = "private" if node.get("tagUsed") == "class" else "public"
            new_stack = namespace_stack + [name] if name else namespace_stack
            for child in node.get("inner", []):
                child_kind = child.get("kind", "")
                if child_kind == "AccessSpecDecl":
                    member_access = child.get("access", member_access)
                elif child_kind == "CXXRecordDecl" and child.get("name") == name:
                    continue  # Skip injected-class-name
                else:
                    walk(child, new_stack, member_access, extern_c)
            return

        # Handle ClassTemplateDecl
        if kind == "ClassTemplateDecl":
            if not in_target or not name or access != "public" or node.get("isImplicit"):
                return
            qualified = build_qualified_name(name, namespace_stack)
            if is_internal_name(qualified):
                return
            comment = find_comment(node)
            has_api, has_brief = get_comment_markers(comment)
            symbols.append(ApiSymbol(
                name=name, qualified_name=qualified, kind="template_class",
                signature=extract_signature(node),
                file=make_relative(effective_file), line=line,
                has_api_marker=has_api, has_brief_doc=has_brief,
            ))
            for child in node.get("inner", []):
                if child.get("kind") == "CXXRecordDecl":
                    walk(child, namespace_stack, access)
            return

        # Handle FunctionTemplateDecl
        if kind == "FunctionTemplateDecl":
            if not in_target or not name or access not in ("public", "protected") or node.get("isImplicit"):
                return
            qualified = build_qualified_name(name, namespace_stack)
            if is_internal_name(qualified):
                return
            inner_func = next(
                (c for c in node.get("inner", [])
                 if c.get("kind") in ("FunctionDecl", "CXXMethodDecl")), None
            )
            type_info = inner_func.get("type", {}).get("qualType", "") if inner_func else ""
            comment = find_comment(node)
            has_api, has_brief = get_comment_markers(comment)
            base_sig = f"{name} : {type_info}" if type_info else name
            signature = enrich_signature(base_sig, inner_func or node, extern_c)
            tmpl_kind = "template_function" if access == "public" else "protected_template_function"
            symbols.append(ApiSymbol(
                name=name, qualified_name=qualified, kind=tmpl_kind,
                signature=signature,
                file=make_relative(effective_file), line=line,
                has_api_marker=has_api, has_brief_doc=has_brief,
            ))
            return

        # Handle TypeAliasTemplateDecl
        if kind == "TypeAliasTemplateDecl":
            if not in_target or not name or access != "public":
                return
            qualified = build_qualified_name(name, namespace_stack)
            if is_internal_name(qualified):
                return
            underlying = ""
            for child in node.get("inner", []):
                if child.get("kind") == "TypeAliasDecl":
                    underlying = child.get("type", {}).get("qualType", "")
                    break
            comment = find_comment(node)
            has_api, has_brief = get_comment_markers(comment)
            symbols.append(ApiSymbol(
                name=name, qualified_name=qualified, kind="template_type_alias",
                signature=f"using {name} = {underlying}" if underlying else f"using {name}",
                file=make_relative(effective_file), line=line,
                has_api_marker=has_api, has_brief_doc=has_brief,
            ))
            return

        # Handle regular declarations
        if kind in ("FunctionDecl", "CXXMethodDecl", "CXXConstructorDecl",
                    "CXXDestructorDecl", "TypeAliasDecl", "TypedefDecl",
                    "EnumDecl", "EnumConstantDecl", "VarDecl", "FieldDecl"):
            if not in_target or not name or access not in ("public", "protected"):
                return
            if node.get("isImplicit"):
                return
            if node.get("explicitlyDeleted"):
                return
            qualified = build_qualified_name(name, namespace_stack)
            if is_internal_name(qualified):
                return
            comment = find_comment(node)
            has_api, has_brief = get_comment_markers(comment)
            decl_kind = map_kind(node)
            if access == "protected":
                decl_kind = "protected_" + decl_kind
            symbols.append(ApiSymbol(
                name=name, qualified_name=qualified, kind=decl_kind,
                signature=enrich_signature(extract_signature(node), node, extern_c),
                file=make_relative(effective_file), line=line,
                has_api_marker=has_api, has_brief_doc=has_brief,
            ))
            if kind == "EnumDecl":
                for child in node.get("inner", []):
                    if child.get("kind") == "EnumConstantDecl":
                        walk(child, namespace_stack + [name], access)
            return

        # Recurse into other nodes
        for child in node.get("inner", []):
            walk(child, namespace_stack, access)

    # Walk top-level AST
    for node in ast_root.get("inner", []):
        if node.get("isImplicit"):
            continue
        walk(node, [], "public")

    # --- Follow type aliases to underlying classes ---
    # Build an index of all class/struct definitions by qualified name
    class_index: dict[str, dict] = {}  # qualified_name -> CXXRecordDecl node

    def index_classes(node: dict, ns_stack: list[str]):
        kind = node.get("kind", "")
        name = node.get("name", "")

        if kind == "NamespaceDecl":
            new_stack = ns_stack + [name] if name else ns_stack
            for child in node.get("inner", []):
                index_classes(child, new_stack)
            return

        if kind == "CXXRecordDecl" and name and node.get("completeDefinition"):
            qualified = build_qualified_name(name, ns_stack)
            class_index[qualified] = node

        if kind == "ClassTemplateDecl" and name:
            qualified = build_qualified_name(name, ns_stack)
            # Find the inner CXXRecordDecl
            for child in node.get("inner", []):
                if child.get("kind") == "CXXRecordDecl" and child.get("completeDefinition"):
                    class_index[qualified] = child
                    break

        for child in node.get("inner", []):
            index_classes(child, ns_stack)

    for node in ast_root.get("inner", []):
        if not node.get("isImplicit"):
            index_classes(node, [])

    # For each type alias that references a class, extract the class's public members
    alias_symbols = [s for s in symbols if s.kind in ("type_alias", "template_type_alias")]
    followed_members: list[ApiSymbol] = []

    def _resolve_class_node(type_name: str, alias_qualified_name: str = "") -> Optional[dict]:
        """Resolve a type name to its CXXRecordDecl in the class index."""
        name = type_name.lstrip(":")
        base = name.split("<")[0].strip()
        if base.startswith("std::") or base.startswith("__"):
            return None
        node = class_index.get(base)
        if not node and alias_qualified_name and "::" in alias_qualified_name:
            alias_namespace = alias_qualified_name.rsplit("::", 1)[0]
            if base.startswith("impl::"):
                node = class_index.get(f"{alias_namespace}::{base}")
            elif "::" not in base:
                node = class_index.get(f"{alias_namespace}::{base}")
        if not node:
            for key, n in class_index.items():
                if key.endswith("::" + base.split("::")[-1]) and base.endswith(key.split("::")[-1]):
                    node = n
                    break
        return node

    def _get_public_members(class_node: dict, visited: Optional[set] = None) -> list[dict]:
        """Get all public method/constructor/destructor members, including inherited ones."""
        if visited is None:
            visited = set()
        node_id = id(class_node)
        if node_id in visited:
            return []
        visited.add(node_id)

        members = []
        member_access = "private" if class_node.get("tagUsed") == "class" else "public"

        for child in class_node.get("inner", []):
            child_kind = child.get("kind", "")
            if child_kind == "AccessSpecDecl":
                member_access = child.get("access", member_access)
                continue

            if member_access != "public":
                continue
            if child.get("isImplicit"):
                continue
            if child.get("explicitlyDeleted"):
                continue

            child_name = child.get("name", "")
            if not child_name:
                continue

            if child_kind in ("CXXMethodDecl", "CXXConstructorDecl", "CXXDestructorDecl", "FunctionTemplateDecl", "FieldDecl"):
                members.append(child)

        # Walk public base classes and include their public members
        for base in class_node.get("bases", []):
            if base.get("access") != "public":
                continue
            base_type = base.get("type", {}).get("qualType", "")
            base_node = _resolve_class_node(base_type)
            if base_node:
                members.extend(_get_public_members(base_node, visited))

        return members

    for alias_sym in alias_symbols:
        # Find the original TypeAliasDecl/TypeAliasTemplateDecl node to get qualType
        underlying_type = _get_underlying_type_for_alias(ast_root, alias_sym.name, alias_sym.qualified_name)
        if not underlying_type:
            continue

        # Skip external/standard library types
        base_name = underlying_type.lstrip(":").split("<")[0].strip()
        if base_name.startswith("std::") or base_name.startswith("__"):
            continue

        # Look up in class index
        class_node = _resolve_class_node(underlying_type, alias_sym.qualified_name)
        if not class_node:
            continue

        # Extract public members (including inherited)
        public_members = _get_public_members(class_node)
        for child in public_members:
            child_kind = child.get("kind", "")
            child_name = child.get("name", "")

            type_info = child.get("type", {}).get("qualType", "")
            attr_node = child
            if child_kind == "FunctionTemplateDecl":
                inner_func = next(
                    (c for c in child.get("inner", [])
                     if c.get("kind") in ("FunctionDecl", "CXXMethodDecl")), None
                )
                if inner_func:
                    type_info = inner_func.get("type", {}).get("qualType", "")
                    attr_node = inner_func

            # Use the alias's qualified name as the parent
            member_qualified = f"{alias_sym.qualified_name}::{child_name}"
            comment = find_comment(child)
            has_api, has_brief = get_comment_markers(comment)

            kind_str = {
                "CXXMethodDecl": "method",
                "CXXConstructorDecl": "constructor",
                "CXXDestructorDecl": "destructor",
                "FunctionTemplateDecl": "template_function",
                "FieldDecl": "field",
            }.get(child_kind, "method")

            base_sig = f"{child_name} : {type_info}" if type_info else child_name
            followed_members.append(ApiSymbol(
                name=child_name,
                qualified_name=member_qualified,
                kind=kind_str,
                signature=enrich_signature(base_sig, attr_node),
                file=alias_sym.file,
                line=alias_sym.line,
                has_api_marker=has_api,
                has_brief_doc=has_brief,
            ))

    # Add followed members to the symbols list
    symbols.extend(followed_members)

    # Sort for deterministic output
    symbols.sort(key=lambda s: (s.file, s.line, s.qualified_name))

    documented = [s for s in symbols if s.has_api_marker]
    undocumented = [s for s in symbols if not s.has_api_marker]

    # Lock file only tracks API shape (no file/line/doc metadata)
    lock_fields = ("name", "qualified_name", "kind", "signature")

    def to_lock_entry(s: ApiSymbol) -> dict:
        d = asdict(s)
        return {k: d[k] for k in lock_fields}

    return ApiSurface(
        target=target_label,
        symbols=[to_lock_entry(s) for s in documented],
        undocumented_symbols=[to_lock_entry(s) for s in undocumented],
    )


def main():
    parser = argparse.ArgumentParser(
        description="Extract public C++ API surface from headers using clang AST"
    )
    parser.add_argument("--ast-json", help="Path to pre-generated clang AST JSON dump")
    parser.add_argument("--target-headers", help="Comma-separated target header paths to filter on")
    parser.add_argument("--target-headers-file", help="Path to newline-separated target header paths")
    parser.add_argument("--target-label", default="", help="Bazel target label (metadata)")
    parser.add_argument("--target", default="", help="Target label (alias for --target-label)")
    parser.add_argument("--output", default="-", help="Output file (- for stdout)")
    args = parser.parse_args()

    def collect_target_files() -> set[str]:
        target_files = set()
        if args.target_headers:
            for h in args.target_headers.split(","):
                h = h.strip()
                if not h:
                    continue
                target_files.add(h)
                target_files.add(os.path.normpath(h))
                target_files.add(os.path.abspath(h))
        if args.target_headers_file:
            with open(args.target_headers_file, "r", encoding="utf-8") as f:
                for line in f:
                    h = line.strip()
                    if not h:
                        continue
                    target_files.add(h)
                    target_files.add(os.path.normpath(h))
                    target_files.add(os.path.abspath(h))
        return target_files

    # Mode 1: Pre-generated AST JSON (Bazel action mode)
    if args.ast_json:
        with open(args.ast_json, "r") as f:
            ast = json.load(f)
        target_files = collect_target_files()
        label = args.target_label or args.target or ""
        surface = extract_from_ast(ast, target_files, label)
    else:
        parser.error("Either --ast-json must be provided")
        return

    result = asdict(surface)
    output_json = json.dumps(result, indent=2, sort_keys=False) + "\n"

    if args.output == "-":
        sys.stdout.write(output_json)
    else:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(output_json)


if __name__ == "__main__":
    main()
