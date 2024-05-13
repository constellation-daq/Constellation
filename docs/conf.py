# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

import sphinx
import pathlib

logger = sphinx.util.logging.getLogger(__name__)

# set directories
docsdir = pathlib.Path(__file__).resolve().parent
repodir = docsdir.parent
srcdir = repodir

# metadata
project = "Constellation"
project_copyright = "2024 DESY and the Constellation authors, CC-BY-4.0"
author = "DESY and the Constellation authors"
version = "0"
release = "v" + version

# extensions
extensions = [
    "ablog",
    "pydata_sphinx_theme",
    "myst_parser",
    "breathe",
    "sphinxcontrib.plantuml",
    "sphinx_design",
    "sphinx_favicon",
]

# general settings
source_suffix = {
    ".rst": "restructuredtext",
    ".md": "markdown",
}

# HTML settings
html_theme = "pydata_sphinx_theme"
html_logo = docsdir.joinpath("logo/logo.png").as_posix()
html_static_path = ["_static", "logo"]
html_theme_options = {
    "logo": {
        "text": project,
    },
    "gitlab_url": "https://gitlab.desy.de/constellation/constellation",
}
html_css_files = [
    "css/custom.css",
]
html_show_sourcelink = False
html_sidebars = {
    # Blog sidebars (https://ablog.readthedocs.io/en/stable/manual/ablog-configuration-options.html#blog-sidebars)
    "news/*": [
        "ablog/postcard.html",
        "ablog/recentposts.html",
        "ablog/categories.html",
        "ablog/archives.html",
    ],
}

# Favicon
favicons = [
    "logo.svg",
]

# myst settings
myst_heading_anchors = 3
myst_fence_as_directive = ["plantuml"]
myst_enable_extensions = ["colon_fence"]
myst_update_mathjax = False

# breathe settings
breathe_projects = {
    "Constellation": docsdir.joinpath("doxygen").joinpath("xml"),
}
breathe_default_project = "Constellation"

# PlantUML settings
plantuml_output_format = "svg_img"

# remove news from toc if news/index.md does not exist
without_news = not docsdir.joinpath("news").joinpath("index.md").exists()
if without_news:
    logger.info("Building documentation without news section", color="yellow")
with open("index.md.in", "rt") as index_in, open("index.md", "wt") as index_out:
    for line in index_in:
        if without_news and "news/index" in line:
            continue
        index_out.write(line)

# ablog settings
blog_path = "news"
blog_post_pattern = ["news/*.md", "news/*.rst"]
post_date_format = "%Y-%m-%d"
