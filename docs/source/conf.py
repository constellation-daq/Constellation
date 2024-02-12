# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

import pathlib

# set directories
docssrcdir = pathlib.Path(__file__).resolve().parent
docsdir = docssrcdir.parent
repodir = docsdir.parent
srcdir = repodir

# metadata
project = "Constellation"
project_copyright = "2023 DESY and the Constellation authors, CC-BY-4.0"
author = "DESY and the Constellation authors"
version = "0"
release = "v" + version

# extensions
extensions = [
    "myst_parser",
    "sphinx_immaterial",
    "sphinx_immaterial.apidoc.cpp.cppreference",
    "sphinx_immaterial.apidoc.cpp.external_cpp_references",
    "breathe",
]

# general settings
source_suffix = {
    ".rst": "restructuredtext",
    ".md": "markdown",
}

# myst settings
myst_heading_anchors = 3

# HTML settings
html_theme = "sphinx_immaterial"
html_logo = docsdir.joinpath("logo.png").as_posix()
html_theme_options = {
    "globaltoc_collapse": True,
    "features": [
        "header.autohide",
        "navigation.expand",
        "navigation.instant",
        "navigation.top",
        "toc.follow",
        "toc.sticky",
    ],
    "palette": [
        {
            "media": "(prefers-color-scheme: light)",
            "scheme": "default",
            "primary": "blue-grey",
            "accent": "orange",
            "toggle": {
                "icon": "material/eye-outline",
                "name": "Switch to dark mode",
            },
        },
        {
            "media": "(prefers-color-scheme: dark)",
            "scheme": "slate",
            "primary": "blue-grey",
            "accent": "orange",
            "toggle": {
                "icon": "material/eye",
                "name": "Switch to light mode",
            },
        },
    ],
    # 'repo_url': 'https://gitlab.desy.de/constellation/constellation',
    # 'repo_name': 'Constellation',
}

# breathe settings
breathe_projects = {
    "Constellation": docsdir.joinpath("doxygen").joinpath("xml"),
}
breathe_default_project = "Constellation"

# external symbols
external_cpp_references = {
    "asio::ip::address": {
        "url": "https://think-async.com/Asio/asio-1.28.0/doc/asio/reference/ip__address.html",
        "object_type": "class",
        "desc": "C++ class",
    },
}
