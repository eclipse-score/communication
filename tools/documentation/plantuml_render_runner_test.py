"""Regression tests for the declared-input PlantUML render runner."""

import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from plantuml_render_runner import RenderError, render, staged_name


SVG = b'<svg xmlns="http://www.w3.org/2000/svg"><text>Example</text></svg>'


class PlantumlRenderRunnerTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        source = self.root / "input.puml"
        source.write_text("@startuml\nstart\n:Example;\nstop\n@enduml\n")
        font = self.root / "font.ttf"
        font.write_bytes(b"test font")
        template = self.root / "fontconfig.properties.tpl"
        template.write_text("font={font_path}\n")
        self.args = argparse.Namespace(
            output=self.root / "output",
            format="svg",
            plantuml=self.root / "plantuml",
            graphviz=self.root / "dot",
            font_template=template,
            font=font,
            source=[["examples/diagram.puml", str(source)]],
            include=[],
        )

    def test_svg_output_and_renderer_contract(self):
        result = subprocess.CompletedProcess([], 0, stdout=SVG, stderr=b"")
        with patch.dict(os.environ, {"RUNFILES_DIR": "/parent/runfiles"}):
            with patch("plantuml_render_runner.subprocess.run", return_value=result) as renderer:
                render(self.args)
        self.assertEqual((self.args.output / "examples/diagram.svg").read_bytes(), SVG)
        command = renderer.call_args.args[0]
        self.assertIn("-pipe", command)
        self.assertIn("-failfast2", command)
        self.assertIn("-tsvg", command)
        self.assertTrue(any(flag.startswith("--jvm_flags=-Djava.awt.headless=true") for flag in command))
        jvm_options = next(flag for flag in command if flag.startswith("--jvm_flags="))
        self.assertIn("-DPLANTUML_SECURITY_PROFILE=ALLOWLIST", jvm_options)
        self.assertIn("-Dplantuml.allowlist.path=", jvm_options)
        self.assertIn("-Dplantuml.allowlist.url=", jvm_options)
        self.assertNotIn("RUNFILES_DIR", renderer.call_args.kwargs["env"])
        self.assertEqual(renderer.call_args.kwargs["env"]["GRAPHVIZ_DOT"], str(self.args.graphviz))
        self.assertEqual(renderer.call_args.kwargs["input"], Path(self.args.source[0][1]).read_bytes())

    def test_renderer_error_includes_source_and_stderr(self):
        result = subprocess.CompletedProcess([], 1, stdout=b"", stderr=b"syntax failure")
        with patch("plantuml_render_runner.subprocess.run", return_value=result):
            with self.assertRaisesRegex(RenderError, "PlantUML failed for examples/diagram.puml") as raised:
                render(self.args)
        self.assertIn("syntax failure", str(raised.exception))

    def test_invalid_svg_is_rejected(self):
        result = subprocess.CompletedProcess([], 0, stdout=b"not XML", stderr=b"")
        with patch("plantuml_render_runner.subprocess.run", return_value=result):
            with self.assertRaisesRegex(RenderError, "Invalid SVG output"):
                render(self.args)

    def test_non_svg_xml_is_rejected(self):
        result = subprocess.CompletedProcess([], 0, stdout=b"<html/>", stderr=b"")
        with patch("plantuml_render_runner.subprocess.run", return_value=result):
            with self.assertRaisesRegex(RenderError, "Output is not SVG"):
                render(self.args)

    def test_png_output_mode(self):
        self.args.format = "png"
        data = b"\x89PNG\r\n\x1a\n" + b"test output"
        result = subprocess.CompletedProcess([], 0, stdout=data, stderr=b"")
        with patch("plantuml_render_runner.subprocess.run", return_value=result) as renderer:
            render(self.args)
        self.assertIn("-tpng", renderer.call_args.args[0])
        self.assertEqual((self.args.output / "examples/diagram.png").read_bytes(), data)

    def test_invalid_png_is_rejected(self):
        self.args.format = "png"
        result = subprocess.CompletedProcess([], 0, stdout=SVG, stderr=b"")
        with patch("plantuml_render_runner.subprocess.run", return_value=result):
            with self.assertRaisesRegex(RenderError, "Output is not PNG"):
                render(self.args)

    def test_declared_include_is_staged_with_source(self):
        include = self.root / "include.puml"
        include.write_text("!define EXAMPLE Example\n")
        self.args.include = [["examples/shared/include.puml", str(include)]]

        def invoke(command, **kwargs):
            self.assertEqual((kwargs["cwd"] / "shared/include.puml").read_text(), include.read_text())
            self.assertTrue(any("-Dplantuml.include.path=" in flag for flag in command))
            return subprocess.CompletedProcess(command, 0, stdout=SVG, stderr=b"")

        with patch("plantuml_render_runner.subprocess.run", side_effect=invoke):
            render(self.args)

    def test_conflicting_staged_inputs_are_rejected(self):
        other = self.root / "other.puml"
        other.write_text("different")
        self.args.include = [["examples/diagram.puml", str(other)]]
        with patch("plantuml_render_runner.subprocess.run") as renderer:
            with self.assertRaisesRegex(RenderError, "Conflicting staged input"):
                render(self.args)
        renderer.assert_not_called()

    def test_escaping_and_absolute_source_names_are_rejected(self):
        for name in ("../../escape.puml", "/absolute.puml", "local/../escape.puml"):
            with self.subTest(name=name):
                with self.assertRaisesRegex(RenderError, "not workspace-relative"):
                    staged_name(name)

    def test_external_repository_source_name(self):
        self.assertEqual(staged_name("../other_repo/diagram.puml"), Path("external/other_repo/diagram.puml"))

    def test_invalid_font_template_is_rejected(self):
        self.args.font_template.write_text("no placeholder")
        with self.assertRaisesRegex(RenderError, "font_path placeholder"):
            render(self.args)


if __name__ == "__main__":
    unittest.main()
