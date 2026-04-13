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
]

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


def setup(app):
    return bazel_sphinx_needs.setup_sphinx_extension(app, needs_external_needs)
