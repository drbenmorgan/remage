import os
import subprocess

on_rtd = os.environ.get('READTHEDOCS', None) == 'True'

if on_rtd:
    subprocess.call('cd ..; doxygen', shell=True)

extensions = [
    "sphinx.ext.mathjax",
    "sphinx.ext.intersphinx",
    "sphinx_copybutton",
    "sphinx_inline_tabs",
    "breathe",
    "myst_parser",
]
breathe_projects = {'remage': '../xml'}
breathe_default_project = "remage"
highlight_language = 'c++'

project = "remage"
copyright = "2020, the LEGEND Collaboration"
author = "Luigi Pertoldi"
version = "0.1.0"

html_logo = '../../.github/logo/remage-logo.png'

# Furo theme
html_theme = "furo"
html_theme_options = {
    "source_repository": "https://github.com/legend-exp/remage",
    "source_branch": "main",
    "source_directory": "docs/source",
}
html_title = f"{project} {version}"
