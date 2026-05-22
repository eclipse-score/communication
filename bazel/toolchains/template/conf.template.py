# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
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

"""
Generic Sphinx configuration template for SCORE modules.

This file is auto-generated from a template and should not be edited directly.
Template variables like {PROJECT_NAME} are replaced during Bazel build.
"""

import bazel_sphinx_needs

from pathlib import Path
import os

from python.runfiles import Runfiles
from sphinx.util import logging

logger = logging.getLogger(__name__)

# Project configuration - {PROJECT_NAME} will be replaced by the module name during build
project = "{PROJECT_NAME}"
author = "S-CORE"
version = "1.0"
release = "1.0.0"
project_url = (
    "https://github.com/eclipse-score"  # Required by score_metamodel extension
)

# Sphinx extensions - comprehensive list for SCORE modules
extensions = [
    "sphinx_needs",
    "sphinx_design",
    "myst_parser",
    "sphinxcontrib.plantuml",
    "breathe",
    "trlc",
]

# -- Breathe configuration --
# Breathe projects can be set via extra_opts using Sphinx -D dot notation:
#   -Dbreathe_projects.project_name=path/to/doxygen/xml
breathe_projects = {}
breathe_default_project = ""
breathe_default_members = ('members',)
breathe_show_define_initializer = True
breathe_show_enumvalue_initializer = True

# MyST parser extensions
myst_enable_extensions = ["colon_fence"]

# Exclude patterns for Bazel builds
exclude_patterns = [
    "bazel-*",
    ".venv*",
]

# Enable markdown rendering
source_suffix = {
    ".rst": "restructuredtext",
    ".md": "markdown",
}

# -- Options for HTML output --
html_theme = 'pydata_sphinx_theme'

# Professional theme configuration inspired by modern open-source projects
html_theme_options = {
    # Navigation settings
    'navigation_depth': 4,
    'collapse_navigation': False,
    'show_nav_level': 2,  # Depth of sidebar navigation
    'show_toc_level': 2,  # Depth of page table of contents

    # Header layout
    'navbar_align': 'left',
    'navbar_start': ['navbar-logo'],
    'navbar_center': ['navbar-nav'],
    'navbar_end': ['navbar-icon-links', 'theme-switcher'],

    # Search configuration
    'search_bar_text': 'Search documentation...',

    # Footer configuration
    'footer_start': ['copyright'],
    'footer_end': ['sphinx-version'],

    # Navigation buttons
    'show_prev_next': True,

    # Logo configuration
    # Sub-doc builds (e.g. dependable_element) are embedded one level deep
    # inside the main docs output.  Add 'link': '../index.html' so the logo
    # navigates back to the main docs.  The main docs build has a 'docs/'
    # directory sibling in the Bazel sandbox; sub-doc builds do not — use that
    # to distinguish the two cases.
    'logo': {
        'text': 'Eclipse S-CORE',
        **({} if (Path(__file__).parent / "docs").is_dir() else {'link': '../index.html'}),
    },

    # External links - S-CORE GitHub
    'icon_links': [
        {
            'name': 'S-CORE GitHub',
            'url': 'https://github.com/eclipse-score',
            'icon': 'fab fa-github',
        }
    ],

}


# Enable numref for cross-references
numfig = True

# Static assets and custom CSS
# html_static_path is relative to confdir (where this conf.py lives).
# In a local build confdir == the docs source dir, so '_static' is a direct
# sibling.  In the Bazel sandbox the generated conf.py sits one level above the
# actual source tree (sphinx_doc/conf.py vs sphinx_doc/docs/sphinx/_static),
# so we search for the _static directory instead of hard-coding the path.
_conf_dir = Path(__file__).parent
_static_local = _conf_dir / "_static"
if _static_local.exists():
    html_static_path = ["_static"]
else:
    _static_found = next(
        (p for p in _conf_dir.rglob("_static") if p.is_dir()), None
    )
    html_static_path = [str(_static_found)] if _static_found else []

# html_css_files is populated after the runfiles section below once we know
# whether default_custom.css is reachable (local _static or runfiles).

# Load external needs and log configuration
needs_external_needs = bazel_sphinx_needs.load_external_needs()
bazel_sphinx_needs.log_config_info(project)

# Resolve the PlantUML binary via Bazel runfiles.
# The plantuml java_binary target is in data of the local sphinx_build binary
# (//bazel/toolchains:sphinx_build), so it is accessible under the _main repo.
r = Runfiles.Create()
if r is None:
    raise ValueError("Could not initialize Bazel runfiles.")

_plantuml_path = None
# Use source_repo="" (the root module's canonical source repo key in repo_mapping)
# so Bazel resolves the apparent name "score_tooling" to the correct canonical
# name regardless of how the dep is declared (local_path_override → "score_tooling+",
# BCR/git_repository → "score_tooling").
_candidate = r.Rlocation("score_tooling/tools/sphinx/plantuml", source_repo="")
if _candidate and Path(_candidate).exists():
    _plantuml_path = Path(_candidate)
    logger.info(f"PlantUML resolved from runfiles: {_plantuml_path}")

if _plantuml_path is None:
    logger.warning(
        "PlantUML binary not found in runfiles — diagrams will not be rendered. "
        "Ensure @score_tooling//tools/sphinx:plantuml is in sphinx_build data."
    )
else:
    # Use PlantUML's built-in Smetana layout engine (Java port of Graphviz).
    # This avoids requiring an external dot binary in the Bazel sandbox.
    plantuml = f"{_plantuml_path} -Playout=smetana"
    plantuml_output_format = "svg_obj"

# Resolve default_custom.css so that sub-doc builds (e.g. dependable_element
# docs) which run in a separate Bazel sandbox also get the project CSS theme.
# The file is declared as data on the sphinx_build binary so it is always
# present in the runfiles tree.
_custom_css_src = None
_css_rloc = r.Rlocation(
    "_main/docs/sphinx/_static/css/default_custom.css", source_repo=""
)
if _css_rloc and Path(_css_rloc).exists():
    _custom_css_src = Path(_css_rloc)
    logger.info(f"Custom CSS resolved from runfiles: {_custom_css_src}")
else:
    # Fallback: already present in the locally discovered _static tree
    for _sp in html_static_path:
        _candidate_css = Path(_sp) / "css" / "default_custom.css"
        if _candidate_css.exists():
            _custom_css_src = _candidate_css
            break

# Only generate the <link> tag when we are certain the file will be available.
html_css_files = ["css/default_custom.css"] if _custom_css_src else []


def _inject_custom_css(app, exception):
    """Write default_custom.css directly into the Sphinx output _static/css/
    directory after the build finishes.

    Using build-finished (rather than builder-inited + tempfile) avoids any
    sandbox write-permission issues in CI: app.outdir is the declared Bazel
    output tree, which is always writable during an action.  Sphinx has already
    run copy_static_files() before this event fires, so our file is not
    overwritten, but the <link> tag generated via html_css_files is already
    present in every page."""
    if exception is not None or _custom_css_src is None:
        return
    import shutil

    css_dir = Path(app.outdir) / "_static" / "css"
    css_dir.mkdir(parents=True, exist_ok=True)
    dest = css_dir / "default_custom.css"
    if not dest.exists():
        shutil.copy(str(_custom_css_src), str(dest))
        logger.info(f"Custom CSS written to output: {dest}")


def setup(app):
    app.connect("build-finished", _inject_custom_css)
    return bazel_sphinx_needs.setup_sphinx_extension(app, needs_external_needs)
