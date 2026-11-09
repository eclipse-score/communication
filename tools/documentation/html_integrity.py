"""Check declared page, diagram, and navigation contracts in a Sphinx HTML tree."""

from __future__ import annotations

import argparse
from dataclasses import dataclass, field
from html.parser import HTMLParser
import json
import os
from pathlib import Path
import sys
from typing import Any
from urllib.parse import unquote, urlsplit
import xml.etree.ElementTree as ET


class IntegrityError(ValueError):
    """A rendered documentation contract is not satisfied."""


@dataclass
class Figure:
    links: list[str] = field(default_factory=list)


@dataclass
class Image:
    reference: str
    figure: Figure | None


class Document(HTMLParser):
    """Collect page anchors, SVG embeddings, and semantic caption references."""

    def __init__(self, content: str):
        super().__init__()
        self.ids: dict[str, int] = {}
        self.images: list[Image] = []
        self.references: list[str] = []
        self.problematic = False
        self.figures: list[Figure] = []
        self.in_caption = False
        self.feed(content)

    def handle_starttag(self, tag: str, attributes: list[tuple[str, str | None]]) -> None:
        attrs = dict(attributes)
        identifier = attrs.get("id")
        if identifier:
            self.ids[identifier] = self.ids.get(identifier, 0) + 1
        classes = (attrs.get("class") or "").split()
        self.problematic |= "problematic" in classes
        if tag == "figure":
            self.figures.append(Figure())
        if tag == "figcaption":
            self.in_caption = True
        if tag == "a" and "reference" in classes:
            reference = attrs.get("href") or ""
            self.references.append(reference)
            if self.in_caption and self.figures:
                self.figures[-1].links.append(reference)
        reference = attrs.get("data") if tag == "object" else attrs.get("src")
        if tag in {"object", "img", "embed"} and reference:
            if urlsplit(reference).path.lower().endswith(".svg"):
                self.images.append(Image(reference, self.figures[-1] if self.figures else None))

    def handle_endtag(self, tag: str) -> None:
        if tag == "figcaption":
            self.in_caption = False
        if tag == "figure" and self.figures:
            self.figures.pop()


def _string(value: Any, context: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise IntegrityError(f"{context}: expected a non-empty string")
    return value


def _strings(value: Any, context: str) -> list[str]:
    if not isinstance(value, list):
        raise IntegrityError(f"{context}: expected a string list")
    result = [_string(entry, context) for entry in value]
    if len(set(result)) != len(result):
        raise IntegrityError(f"{context}: duplicate expected value")
    return result


class Site:
    """Resolve local resources against their real HTML or SVG document base."""

    def __init__(self, root: Path):
        self.root = Path(os.path.abspath(root))
        if not self.root.is_dir():
            raise IntegrityError(f"HTML tree does not exist: {root}")
        self.documents: dict[Path, Document] = {}
        self.svg: dict[Path, ET.Element] = {}

    def document(self, path: Path) -> Document:
        if path not in self.documents:
            self.documents[path] = Document(path.read_text(encoding="utf-8"))
        return self.documents[path]

    def image(self, path: Path) -> ET.Element:
        if path not in self.svg:
            try:
                self.svg[path] = ET.parse(path).getroot()
            except ET.ParseError as error:
                raise IntegrityError(f"Invalid SVG {path}: {error}") from error
            if self.svg[path].tag.rsplit("}", 1)[-1] != "svg":
                raise IntegrityError(f"Resource is not an SVG: {path}")
        return self.svg[path]

    def local_target(self, source: Path, reference: str) -> tuple[Path, str] | None:
        target = urlsplit(reference)
        if target.scheme or target.netloc:
            return None
        decoded = unquote(target.path)
        if not decoded:
            path = source
        elif decoded.startswith("/"):
            path = self.root / decoded.lstrip("/")
        else:
            path = source.parent / decoded
        path = Path(os.path.abspath(path))
        if not path.is_relative_to(self.root):
            raise IntegrityError(f"Local reference escapes HTML tree: {source}: {reference}")
        if path.is_dir():
            path /= "index.html"
        if not path.is_file():
            raise IntegrityError(f"Missing local resource: {source}: {reference}")
        return path, unquote(target.fragment)

    def check_reference(self, source: Path, reference: str) -> None:
        target = self.local_target(source, reference)
        if target is None:
            return
        path, fragment = target
        if not fragment:
            return
        if path.suffix.lower() == ".html":
            found = self.document(path).ids.get(fragment, 0)
        elif path.suffix.lower() == ".svg":
            found = sum(element.get("id") == fragment for element in self.image(path).iter())
        else:
            return
        if found != 1:
            raise IntegrityError(f"Unresolved or duplicate anchor: {source}: {reference}")

    def page(self, anchor: str) -> tuple[Path, Document]:
        matches = []
        for path in sorted(self.root.rglob("*.html")):
            document = self.document(path)
            if anchor in document.ids:
                matches.append((path, document))
        if len(matches) != 1:
            raise IntegrityError(f"Page anchor {anchor!r}: expected one page, found {len(matches)}")
        path, document = matches[0]
        if document.ids[anchor] != 1:
            raise IntegrityError(f"Duplicate page anchor {anchor!r}: {path}")
        return path, document


def validate_site(root: Path, expectations: dict[str, Any]) -> int:
    """Validate configured pages; return the number of page contracts checked."""
    if not isinstance(expectations, dict) or expectations.get("schema_version") != 1:
        raise IntegrityError("Expected documentation contract schema_version 1")
    pages = expectations.get("pages")
    if not isinstance(pages, list) or not pages:
        raise IntegrityError("Expected at least one page contract")
    site = Site(root)
    for contract in pages:
        if not isinstance(contract, dict):
            raise IntegrityError("Page contract must be an object")
        page_anchor = _string(contract.get("anchor"), "page anchor")
        path, document = site.page(page_anchor)
        if document.problematic:
            raise IntegrityError(f"Unresolved Sphinx markup on {path}")
        for anchor in _strings(contract.get("required_anchors", []), "required anchors"):
            if document.ids.get(anchor, 0) != 1:
                raise IntegrityError(f"Missing or duplicate required anchor {anchor!r}: {path}")
        for reference in document.references:
            site.check_reference(path, reference)
        images = []
        for embedded in document.images:
            target = site.local_target(path, embedded.reference)
            if target is None:
                raise IntegrityError(f"Expected a local SVG resource: {embedded.reference}")
            image_path, _fragment = target
            image = site.image(image_path)
            text = " ".join(" ".join(image.itertext()).split())
            if "Syntax Error" in text:
                raise IntegrityError(f"PlantUML error image: {image_path}")
            for element in image.iter():
                if element.tag.rsplit("}", 1)[-1] == "a":
                    reference = element.get("href", element.get("{http://www.w3.org/1999/xlink}href", ""))
                    site.check_reference(image_path, reference)
            images.append((embedded, text))
        diagrams = contract.get("diagrams", [])
        if not isinstance(diagrams, list):
            raise IntegrityError("Expected a diagram contract list")
        for diagram in diagrams:
            if not isinstance(diagram, dict):
                raise IntegrityError("Diagram contract must be an object")
            title = _string(diagram.get("title"), "diagram title")
            matches = [embedded for embedded, text in images if title in text]
            if len(matches) != 1:
                raise IntegrityError(f"Diagram {title!r}: expected one embedding, found {len(matches)} on {path}")
            if "caption_targets" in diagram:
                expected = _strings(diagram["caption_targets"], "caption targets")
                actual = matches[0].figure.links if matches[0].figure is not None else []
                if sorted(actual) != sorted(expected):
                    raise IntegrityError(f"Diagram {title!r}: caption targets {actual!r} differ from {expected!r}")
    return len(pages)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--site", type=Path, required=True)
    parser.add_argument("--expectations", type=Path, required=True)
    args = parser.parse_args()
    try:
        expectations = json.loads(args.expectations.read_text(encoding="utf-8"))
        checked = validate_site(args.site, expectations)
    except (IntegrityError, OSError, ValueError) as error:
        print(f"Documentation integrity check failed: {error}", file=sys.stderr)
        return 1
    print(f"Validated {checked} documentation page contract(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
