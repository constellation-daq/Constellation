# SPDX-FileCopyrightText: 2023 Stephan Lachnit <stephanlachnit@debian.org>
# SPDX-License-Identifier: EUPL-1.2

import json
import pathlib

from sphinx.util import logging

# set directories
docssrcdir = pathlib.Path(__file__).resolve().parent
docsdir = docssrcdir.parent
repodir = docsdir.parent
srcdir = repodir

# metadata
project = 'CHIRP'
project_copyright = '2023 Stephan Lachnit, CC-BY-SA-4.0'
author = 'Stephan Lachnit'
version = '0'
release = '0'

# extensions
extensions = [
    'myst_parser',
    'hawkmoth',
    'hawkmoth.ext.javadoc',
    'sphinx_immaterial',
    'sphinx_immaterial.apidoc.cpp.cppreference',
    'sphinx_immaterial.apidoc.cpp.external_cpp_references',
]

# general settings
source_suffix = {
    '.rst': 'restructuredtext',
    '.md': 'markdown',
}

# HTML settings
html_theme = 'sphinx_immaterial'
html_logo = docsdir.joinpath('logo.png').as_uri()
html_theme_options = {
    'globaltoc_collapse': True,
    'features': [
        'header.autohide',
        'navigation.expand',
        'navigation.instant',
        'navigation.top',
        'toc.follow',
        'toc.sticky',
    ],
    'palette': [
        {
            'media': '(prefers-color-scheme: light)',
            'scheme': 'default',
            'primary': 'blue-grey',
            'accent': 'orange',
            'toggle': {
                'icon': 'material/eye-outline',
                'name': 'Switch to dark mode',
            }
        },
        {
            'media': '(prefers-color-scheme: dark)',
            'scheme': 'slate',
            'primary': 'blue-grey',
            'accent': 'orange',
            'toggle': {
                'icon': 'material/eye',
                'name': 'Switch to light mode',
            }
        },
    ],
    # 'repo_url': 'https://gitlab.desy.de/constellation/constellation',
    # 'repo_name': 'Constellation',
}

# hawkmoth settings
hawkmoth_root = srcdir
hawkmoth_transform_default = 'javadoc'
hawkmoth_clang = ['-std=c++23']

# add include dirs from meson
meson_intro_dep = repodir.joinpath('builddir', 'meson-info', 'intro-dependencies.json')
if meson_intro_dep.exists():
    with open(meson_intro_dep, mode='rt', encoding='utf-8') as meson_intro_dep_file:
        intro_dep = json.load(meson_intro_dep_file)
        for dep in intro_dep:
            if 'include_directories' in dep:
                for path in dep['include_directories']:
                    hawkmoth_clang.append(f'-I{path}')
else:
    logger = logging.getLogger(__name__)
    logger.warn(f'Could not find valid "builddir" in {repodir.as_posix()} to extract include directories from meson')

# asio symbols
external_cpp_references = {
    "asio::ip::address": {
        "url": "https://think-async.com/Asio/asio-1.28.0/doc/asio/reference/ip__address.html",
        "object_type": "class",
        "desc": "C++ class",
    },
}
