"""Render declared PlantUML inputs with declared runtime resources."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


class RenderError(ValueError):
    """PlantUML inputs or renderer output did not satisfy the render contract."""


def staged_name(short_path: str) -> Path:
    if short_path.startswith("../"):
        short_path = "external/" + short_path[3:]
    path = Path(short_path)
    if path.is_absolute() or ".." in path.parts:
        raise RenderError(f"Input path is not workspace-relative: {short_path}")
    return path


def render(args: argparse.Namespace) -> None:
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    plantuml = args.plantuml.resolve()
    graphviz = args.graphviz.resolve()
    with tempfile.TemporaryDirectory(prefix="plantuml-render-") as directory:
        staging = Path(directory)
        inputs = args.source + args.include
        staged = {}
        for short_path, source_path in inputs:
            relative = staged_name(short_path)
            source = Path(source_path).resolve()
            if relative in staged and staged[relative] != source:
                raise RenderError(f"Conflicting staged input: {relative}")
            destination = staging / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, destination)
            staged[relative] = source
        font = staging / "fallback.ttf"
        shutil.copyfile(args.font, font)
        font_config = staging / "fontconfig.properties"
        template = args.font_template.read_text(encoding="utf-8")
        if "{font_path}" not in template:
            raise RenderError("Font configuration template has no font_path placeholder")
        font_config.write_text(template.replace("{font_path}", str(font)), encoding="utf-8")
        include_dirs = sorted({str((staging / staged_name(short_path)).parent) for short_path, _path in args.include})
        jvm_flags = [
            "-Djava.awt.headless=true",
            f"-Dsun.awt.fontconfig={font_config}",
            "-DPLANTUML_SECURITY_PROFILE=ALLOWLIST",
            f"-Dplantuml.allowlist.path={staging}",
            "-Dplantuml.allowlist.url=",
        ]
        if include_dirs:
            jvm_flags.append("-Dplantuml.include.path=" + os.pathsep.join(include_dirs))
        environment = os.environ.copy()
        for key in ("RUNFILES_DIR", "RUNFILES_MANIFEST_FILE", "JAVA_RUNFILES"):
            environment.pop(key, None)
        environment["GRAPHVIZ_DOT"] = str(graphviz)
        command = [
            str(plantuml),
            "--jvm_flags=" + " ".join(jvm_flags),
            "-graphvizdot",
            str(graphviz),
            "-t" + args.format,
            "-failfast2",
            "-pipe",
        ]
        for short_path, _source_path in args.source:
            relative = staged_name(short_path)
            source = staging / relative
            result = subprocess.run(
                command,
                input=source.read_bytes(),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                cwd=source.parent,
                env=environment,
                check=False,
            )
            if result.returncode:
                message = result.stderr.decode("utf-8", errors="replace")
                raise RenderError(f"PlantUML failed for {short_path}:\n{message}")
            if args.format == "svg":
                try:
                    document = ET.fromstring(result.stdout)
                except ET.ParseError as error:
                    raise RenderError(f"Invalid SVG output for {short_path}: {error}") from error
                if document.tag.rsplit("}", 1)[-1] != "svg":
                    raise RenderError(f"Output is not SVG for {short_path}")
            elif not result.stdout.startswith(b"\x89PNG\r\n\x1a\n"):
                raise RenderError(f"Output is not PNG for {short_path}")
            destination = output / relative.with_suffix("." + args.format)
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(result.stdout)
    print(f"Rendered {len(args.source)} PlantUML source(s) as {args.format}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--format", choices=("svg", "png"), default="svg")
    parser.add_argument("--plantuml", type=Path, required=True)
    parser.add_argument("--graphviz", type=Path, required=True)
    parser.add_argument("--font-template", type=Path, required=True)
    parser.add_argument("--font", type=Path, required=True)
    parser.add_argument("--source", action="append", nargs=2, required=True)
    parser.add_argument("--include", action="append", nargs=2, default=[])
    args = parser.parse_args()
    try:
        render(args)
    except (RenderError, OSError) as error:
        print(f"PlantUML render check failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
