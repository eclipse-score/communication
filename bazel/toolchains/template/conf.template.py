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
    'logo': {
        'text': 'Eclipse S-CORE',
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
    plantuml = str(_plantuml_path)
    plantuml_output_format = "svg_obj"


def setup(app):
    return bazel_sphinx_needs.setup_sphinx_extension(app, needs_external_needs)
