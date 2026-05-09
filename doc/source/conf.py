# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'ACNanoLiveLens'
copyright = '2025-2026, AcFun-FOSS'
author = 'UNTITLED'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = ["sphinxcontrib.pseudocode2", "sphinxcontrib.drawio"]

templates_path = ['_templates']
exclude_patterns = []

language = 'zh_CN'

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_static_path = ['_static']
#html_theme = 'shibuya'
html_theme = 'bizstyle'

if html_theme == 'bizstyle':
    html_css_files = [
        'css/bizstyle_custom.css'
    ]

# -- Options for pseudocode2 -------------------------------------------------
# https://github.com/DeepPSP/sphinxcontrib-pseudocode2

pseudocode2_options = {
    "lineNumber": True,           # Global default: enable line numbering
    "lineNumberPunc": " | ",      # Punctuation after line numbers (e.g., "1 | ")
    "commentDelimiter": "#",      # Global default comment delimiter
    "noEnd": False,               # Global default: show "END" for control blocks
    "titlePrefix": "PseudoCode",  # Global default title prefix (replace "Algorithm")
    "scopeLines": True,           # Global default: enable scope line highlighting
}

drawio_builder_export_format = {
	"html": "svg",
	"latex": "pdf"
}
drawio_default_export_scale = 110
