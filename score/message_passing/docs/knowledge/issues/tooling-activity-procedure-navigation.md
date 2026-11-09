# Activity Procedure Navigation

Status: open
Area: tooling
Scope: activity procedure references; score_tooling 3.0.2 and its pinned Sphinx renderer
Owner: unassigned
Evidence checked: 2026-11-06; source inspection, Sphinx builds, and rendered HTML/SVG checks

## Finding

Separate procedure activities are supported, but a manually authored
`[[#section Procedure()]]` link does not provide correct parent-page navigation
with the current SVG-object output. The fragment is resolved against the SVG
file in `_images`, even when the link targets the top-level browsing context.

The pinned Sphinx PlantUML directive provides `png`, `svg`, `svg_img`, and
`svg_obj` modes. `svg` is an object with a PNG fallback, not inline SVG.
`png` emits an image without a clickable image map. A per-diagram format switch
therefore does not make the manual page-fragment link usable.

This is a navigation limitation, not a rejection of separate activity files
or a change to the shared procedure's runtime semantics.

## Evidence and Current Pattern

Run from the communication repository root:

```sh
bazel build //score/message_passing:client_connection
bazel build //docs/sphinx:sphinx_doc
```

The [unit design](../../../dependability/software_unit_design/client_connection.rst)
defines the named target `client-connection-switch-to-stop-state`, embeds the
[shared finalization activity](../../../dependability/software_unit_design/client_connection_switch_to_stop_state_activity.puml)
once, and links both caller captions to it using Sphinx `:ref:` roles. The
caller activity nodes retain plain `SwitchToStopState()` calls.

Rendered HTML/SVG checks confirmed one rendering of each activity, a shared
section target, both caller-caption links pointing to that target, and no
remaining unusable SVG-object fragment links. Caption references remain
available for future input-failure and peer-termination views.

The pinned `plantuml/sphinx/clickable_plantuml/clickable_plantuml.py` builds
image-relative URLs for automatically resolved entity references in SVG-object
mode. The manual activity call links did not go through that resolution path.
The pinned `sphinxcontrib.plantuml` directive's `option_spec`, HTML format table,
and PNG/SVG tag builders establish the rendering behavior above.

No renderer cache or repository-wide configuration was modified.

## Next Check

Recheck on a tooling update whether named activity procedure references can
resolve portable document targets for the actual embedding mode. Until then,
use named procedure calls plus caption references rather than hard-coding
generated HTML paths or treating a bare SVG fragment as a parent-page link.