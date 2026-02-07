# Configuration file for the Sphinx documentation builder.

project = 'CFGPack'
copyright = '2026, CFGPack Contributors'
author = 'CFGPack Contributors'

# -- General configuration ---------------------------------------------------

extensions = [
    'breathe',
    'myst_parser',
]

# Breathe configuration (for Doxygen XML integration)
# Doxygen outputs to build/docs/doxygen/xml, Sphinx runs from docs/source
breathe_projects = {
    'CFGPack': '../../build/docs/doxygen/xml'
}
breathe_default_project = 'CFGPack'

# MyST configuration (for Markdown support)
myst_enable_extensions = [
    'colon_fence',
    'deflist',
]

# Source file suffixes
source_suffix = {
    '.rst': 'restructuredtext',
    '.md': 'markdown',
}

templates_path = ['_templates']
exclude_patterns = []

# Suppress warnings for duplicate declarations (from anonymous struct members)
suppress_warnings = ['duplicate_declaration.cpp']

# -- Options for HTML output -------------------------------------------------

html_theme = 'sphinx_rtd_theme'
html_static_path = ['_static']

# Create _static directory if it doesn't exist (avoids warning)
import os
os.makedirs(os.path.join(os.path.dirname(__file__), '_static'), exist_ok=True)
