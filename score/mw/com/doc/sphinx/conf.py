# ******************************************************************************
# Copyright (c) 2026 Contributors to the Eclipse Foundation
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

"""Sample Configuration file for the Sphinx documentation builder.

This example demonstrates how to create professional, elegant documentation
for the Eclipse S-CORE.
"""

import os
import sys
import warnings
from pathlib import Path

# -- Project information --
project = 'Eclipse S-CORE'
copyright = '2026, Eclipse S-CORE Contributors'
author = 'Eclipse Foundation'
release = '1.0.0'

# -- General configuration ---
extensions = [
    'sphinx.ext.autodoc',      # Auto-generate documentation from docstrings
    'sphinx.ext.napoleon',     # Support for NumPy and Google style docstrings
    'sphinx.ext.viewcode',     # Add links to highlighted source code
    'sphinx.ext.intersphinx',  # Link to other project documentation
    'sphinx.ext.todo',         # Support TODO directives
    'breathe',                 # Bridge between Doxygen and Sphinx
    'myst_parser',             # Support for Markdown files
]

# MyST Parser configuration
source_suffix = {
    '.rst': 'restructuredtext',
    '.md': 'markdown',
}

# MyST parser settings to handle broken links in included markdown files
myst_heading_anchors = 3
myst_enable_extensions = [
    # "linkify",  # Disabled - requires linkify-it-py package
]
# Suppress warnings for unknown references in markdown files
# These references point to files outside the sphinx documentation
suppress_warnings = [
    'myst.xref_missing',
    'myst.anchor',
]
# Continue on errors from myst parser for included markdown files
myst_commonmark_only = False
myst_all_links_external = False

# Add any paths that contain templates here
templates_path = ['_templates']

# List of patterns to ignore when looking for source files
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

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

# Add custom styling
html_static_path = ['_static']
html_css_files = [
    'css/default_custom.css',
]

# -- Breathe configuration --
# Doxygen XML output path (provided by sphinx_docs_library)
# Path is relative to each .rst file location
breathe_projects = {
    "com": "../../design/doxygen_build/xml",
}

# Set the default project for breathe directives
breathe_default_project = "com"

# Breathe display options for better readability
breathe_default_members = ('members',)
breathe_show_define_initializer = True
breathe_show_enumvalue_initializer = True
breathe_domain_by_extension = {
    "h": "cpp",
    "hpp": "cpp",
    "cpp": "cpp",
    "cc": "cpp",
    "cxx": "cpp",
    "c": "c",
}
breathe_implementation_filename_extensions = ['.c', '.cc', '.cpp']
breathe_order_parameters_first = True

# Performance optimization - limit what gets processed
breathe_projects_source = {}
breathe_build_directory = ""


warnings.filterwarnings("ignore", message=".*breathe.*")

# Some performance settings
html_show_sourcelink = False
html_copy_source = False
