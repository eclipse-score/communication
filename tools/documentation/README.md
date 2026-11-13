# Documentation Checks

This pilot provides reusable rendering and HTML integrity checks. It does not
replace architecture validation, C++ design comparison, or human visual review.

## Client Pilot

```sh
bazel build //score/message_passing/dependability/software_unit_design:client_connection_render
bazel test //score/message_passing/dependability/software_unit_design:client_connection_html_test
bazel test //tools/documentation:html_integrity_test //tools/documentation:plantuml_render_runner_test
```

The render command prints its output directory; SVG paths beneath it preserve
the source workspace-relative paths. It is a fast iteration check, separate
from Sphinx. The HTML test consumes
the existing Sphinx artifact; Bazel builds that artifact only when needed.
Do not rerender every diagram separately after an unchanged Sphinx output
has already been checked.

The HTML pilot is tagged `manual` and must be invoked explicitly. Its adoption
as an automatic documentation CI gate is a separate decision; it does not add
the full Sphinx build to unrelated recursive runtime test runs.

## Rendering

Load `plantuml_render` from [defs.bzl](defs.bzl). Declare `.puml` sources,
any include/assets in `includes`, and optionally `format = "png"` rather than
the default `svg`. Each source contains one diagram and produces one output
using its workspace-relative path. Named `@startuml` blocks do not change that
output path. External inputs are staged under an `external` directory.

The action uses the pinned `score_tooling` PlantUML executable, bundled fallback
font configuration, and hermetic Graphviz wrapper. It declares those inputs and
tool runfiles, stages only supplied sources/includes, fails on renderer errors,
and checks the output container before publishing it. PlantUML's allowlist
profile restricts file includes to that staging tree and permits no URL
includes. Missing include inputs must be declared, not recovered from a
workspace or dependency cache.

## HTML Contracts

Load `html_integrity_test` from [defs.bzl](defs.bzl), supplying a Sphinx target
as `site` and a JSON file as `expectations`. The site target must expose one HTML
tree and permit the test package to consume it.

[The client contract](../../score/message_passing/dependability/software_unit_design/client_connection_html_expectations.json)
is an example. `schema_version` is `1`; each `pages` entry selects a unique page
by `anchor`, optionally requires more anchors, and declares `diagrams`.
A diagram's `title` is a unique visible-text selector, normally its title;
an existing class view can use a distinctive label without changing the design.
Each declared diagram must be embedded once. Optional `caption_targets` declares
the exact semantic caption references; Sphinx figure permalinks are excluded.

The checker parses HTML and SVG, verifies local resources and HTML/SVG anchors,
rejects unresolved Sphinx markup and renderer error images, and resolves SVG
links relative to the SVG document, not its containing page. Logical site paths
are preserved when Bazel runfiles use symlinked files. External URL content is
not fetched or validated. Current diagram contracts support SVG embeddings via
`object`, `img`, or `embed`; they do not inspect PNG pixels.

Contracts do not pin generated HTML paths, SVG hashes, runtime test counts, or
C++ operation inventories. Requirements coverage and implementation fidelity
remain separate checks. A passing contract is not concurrency or safety
acceptance, and visual readability still needs a reviewer.