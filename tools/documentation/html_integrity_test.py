"""Regression tests for rendered documentation integrity contracts."""

from pathlib import Path
import tempfile
import unittest

from html_integrity import IntegrityError, validate_site


class HtmlIntegrityTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        (self.root / "_images").mkdir()
        (self.root / "nested").mkdir()
        self.page = self.root / "nested" / "index.html"
        self.image = self.root / "_images" / "diagram.svg"
        self.image.write_text('<svg xmlns="http://www.w3.org/2000/svg"><text>Example procedure</text></svg>')
        self.page.write_text(
            '<section id="page-contract"><section id="shared-procedure"></section>'
            '<figure id="figure-1"><object data="../_images/diagram.svg" type="image/svg+xml"></object>'
            '<figcaption><a class="reference internal" href="#shared-procedure">Shared</a>'
            '<a class="headerlink" href="#figure-1">Permalink</a></figcaption></figure></section>'
        )
        self.contract = {
            "schema_version": 1,
            "pages": [
                {
                    "anchor": "page-contract",
                    "required_anchors": ["shared-procedure"],
                    "diagrams": [{"title": "Example procedure", "caption_targets": ["#shared-procedure"]}],
                }
            ],
        }

    def test_page_anchor_and_caption_ignore_figure_permalink(self):
        self.assertEqual(validate_site(self.root, self.contract), 1)

    def test_declared_tree_symlink_files_keep_logical_url_base(self):
        actual = tempfile.TemporaryDirectory()
        self.addCleanup(actual.cleanup)
        actual_root = Path(actual.name)
        actual_page = actual_root / "page.html"
        actual_image = actual_root / "image.svg"
        actual_page.write_text(self.page.read_text())
        actual_image.write_text(self.image.read_text())
        self.page.unlink()
        self.image.unlink()
        self.page.symlink_to(actual_page)
        self.image.symlink_to(actual_image)
        self.assertEqual(validate_site(self.root, self.contract), 1)

    def test_missing_diagram_fails(self):
        self.contract["pages"][0]["diagrams"][0]["title"] = "Absent diagram"
        with self.assertRaisesRegex(IntegrityError, "found 0"):
            validate_site(self.root, self.contract)

    def test_duplicate_embedding_fails(self):
        with self.page.open("a") as output:
            output.write('<object data="../_images/diagram.svg"></object>')
        with self.assertRaisesRegex(IntegrityError, "found 2"):
            validate_site(self.root, self.contract)

    def test_caption_contract_mismatch_fails(self):
        self.contract["pages"][0]["diagrams"][0]["caption_targets"] = []
        with self.assertRaisesRegex(IntegrityError, "caption targets"):
            validate_site(self.root, self.contract)

    def test_unresolved_html_reference_fails(self):
        self.page.write_text(self.page.read_text().replace('href="#shared-procedure"', 'href="#absent"'))
        with self.assertRaisesRegex(IntegrityError, "Unresolved or duplicate anchor"):
            validate_site(self.root, self.contract)

    def test_svg_object_fragment_uses_svg_document_base(self):
        self.image.write_text(
            '<svg xmlns="http://www.w3.org/2000/svg"><text>Example procedure</text>'
            '<a href="#shared-procedure" target="_top"><text>Shared</text></a></svg>'
        )
        with self.assertRaisesRegex(IntegrityError, "Unresolved or duplicate anchor"):
            validate_site(self.root, self.contract)

    def test_svg_image_relative_html_reference_passes(self):
        self.image.write_text(
            '<svg xmlns="http://www.w3.org/2000/svg"><text>Example procedure</text>'
            '<a href="../nested/index.html#shared-procedure"><text>Shared</text></a></svg>'
        )
        self.assertEqual(validate_site(self.root, self.contract), 1)

    def test_svg_local_anchor_passes(self):
        self.image.write_text(
            '<svg xmlns="http://www.w3.org/2000/svg"><text id="local">Example procedure</text>'
            '<a href="#local"><text>Local</text></a></svg>'
        )
        self.assertEqual(validate_site(self.root, self.contract), 1)

    def test_duplicate_page_anchor_fails(self):
        (self.root / "another.html").write_text('<section id="page-contract"></section>')
        with self.assertRaisesRegex(IntegrityError, "expected one page, found 2"):
            validate_site(self.root, self.contract)

    def test_duplicate_required_anchor_fails(self):
        with self.page.open("a") as output:
            output.write('<span id="shared-procedure"></span>')
        with self.assertRaisesRegex(IntegrityError, "duplicate required anchor"):
            validate_site(self.root, self.contract)

    def test_missing_svg_fails(self):
        self.image.unlink()
        with self.assertRaisesRegex(IntegrityError, "Missing local resource"):
            validate_site(self.root, self.contract)

    def test_invalid_svg_fails(self):
        self.image.write_text("<svg>")
        with self.assertRaisesRegex(IntegrityError, "Invalid SVG"):
            validate_site(self.root, self.contract)

    def test_error_image_fails(self):
        self.image.write_text('<svg xmlns="http://www.w3.org/2000/svg"><text>Syntax Error</text></svg>')
        with self.assertRaisesRegex(IntegrityError, "PlantUML error image"):
            validate_site(self.root, self.contract)

    def test_escaping_resource_fails(self):
        self.page.write_text(self.page.read_text().replace("../_images/diagram.svg", "../../../outside.svg"))
        with self.assertRaisesRegex(IntegrityError, "escapes HTML tree"):
            validate_site(self.root, self.contract)

    def test_problematic_markup_fails(self):
        with self.page.open("a") as output:
            output.write('<span class="problematic">Unresolved</span>')
        with self.assertRaisesRegex(IntegrityError, "Unresolved Sphinx markup"):
            validate_site(self.root, self.contract)

    def test_external_reference_does_not_require_network(self):
        with self.page.open("a") as output:
            output.write('<a class="reference external" href="https://example.invalid/doc">External</a>')
        self.assertEqual(validate_site(self.root, self.contract), 1)

    def test_unknown_schema_fails(self):
        self.contract["schema_version"] = 2
        with self.assertRaisesRegex(IntegrityError, "schema_version"):
            validate_site(self.root, self.contract)


if __name__ == "__main__":
    unittest.main()
