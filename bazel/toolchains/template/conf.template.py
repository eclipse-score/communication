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

from pathlib import Path

from sphinx.util import logging

import sphinx_conf_helpers

logger = logging.getLogger(__name__)

# Project configuration - {PROJECT_NAME} will be replaced by the module name during build
project = "{PROJECT_NAME}"
author = "S-CORE"
version = "1.0"
release = "1.0.0"
project_url = "https://github.com/eclipse-score"  # Required by score_metamodel extension

# Sphinx extensions - comprehensive list for SCORE modules
extensions = [
    "sphinx_module_ext",
    "sphinx_needs",
    "sphinx_design",
    "myst_parser",
    "sphinxcontrib.plantuml",
    "clickable_plantuml",
    "breathe",
    "trlc",
]

# -- Breathe configuration --
# Breathe projects can be set via extra_opts using Sphinx -D dot notation:
#   -Dbreathe_projects.project_name=path/to/doxygen/xml
breathe_projects = {}
breathe_default_project = ""
breathe_default_members = ("members",)
breathe_show_define_initializer = True
breathe_show_enumvalue_initializer = True

# MyST parser extensions
myst_enable_extensions = sphinx_conf_helpers.DEFAULT_MYST_ENABLE_EXTENSIONS

# Exclude patterns for Bazel builds
exclude_patterns = sphinx_conf_helpers.DEFAULT_EXCLUDE_PATTERNS

# Enable markdown rendering
source_suffix = {
    ".rst": "restructuredtext",
    ".md": "markdown",
}

# -- Options for HTML output --
html_theme = "pydata_sphinx_theme"

html_css_files = ["css/default_custom.css"]

# Professional theme configuration inspired by modern open-source projects
html_theme_options = {
    # Navigation settings
    "navigation_depth": 4,
    "collapse_navigation": False,
    "show_nav_level": 2,  # Depth of sidebar navigation
    "show_toc_level": 2,  # Depth of page table of contents
    # Header layout
    "navbar_align": "left",
    "navbar_start": ["navbar-logo"],
    "navbar_center": ["navbar-nav"],
    "navbar_end": ["navbar-icon-links", "theme-switcher"],
    # Search configuration
    "search_bar_text": "Search documentation...",
    # Footer configuration
    "footer_start": ["copyright"],
    "footer_end": ["sphinx-version"],
    # Navigation buttons
    "show_prev_next": True,
    # Logo configuration
    "logo": {
        "text": "Eclipse S-CORE",
        **({} if (Path(__file__).parent / "docs").is_dir() else {"link": "../index.html"}),
    },
    # External links - S-CORE GitHub
    "icon_links": [
        {
            "name": "S-CORE GitHub",
            "url": "https://github.com/eclipse-score/communication",
            "icon": "fab fa-github",
        }
    ],
}


# Enable numref for cross-references
numfig = True

# Needs external-needs loading and config logging are now handled by the
# "sphinx_module_ext" extension registered above (see
# @score_tooling//bazel/rules/rules_score:src/sphinx_module_ext.py) instead of
# calling bazel_sphinx_needs directly.
suppress_warnings = sphinx_conf_helpers.DEFAULT_SUPPRESS_WARNINGS + ["myst.xref_missing"]

# Hermetic PlantUML / Graphviz / FTA-metamodel resolution.
# PLANTUML_BIN, GRAPHVIZ_DOT and FTA_METAMODEL_DIR are injected into every
# sphinx_module build action by sphinx_module.bzl itself, so no Bazel
# runfiles lookup is needed here anymore -- see sphinx_conf_helpers'
# module docstring for details.
_graphviz_dot = sphinx_conf_helpers.resolve_graphviz_dot()
plantuml_output_format = "svg_obj"
plantuml = sphinx_conf_helpers.resolve_plantuml_command(graphviz_dot_path=_graphviz_dot)
graphviz_output_format = "svg"
