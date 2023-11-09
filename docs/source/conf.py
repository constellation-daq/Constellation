
import pathlib

# set directories
docssrcdir = pathlib.Path(__file__).resolve().parent
repodir = docssrcdir.parents[1]
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
]

# general settings
source_suffix = {
    '.rst': 'restructuredtext',
    '.md': 'markdown',
}

# HTML settings
html_theme = 'sphinx_immaterial'
html_theme_options = {
    'globaltoc_collapse': True,
    'features': [
        'navigation.expand',
        'navigation.sections',
        'navigation.top',
        'toc.follow',
        'toc.sticky',
    ],
}

# hawkmoth settings
hawkmoth_root = srcdir
hawkmoth_transform_default = 'javadoc'
hawkmoth_clang = [
    '-std=c++23',
    # inclues should be auto generated from builddir/meson-info/intro-dependencies.json
    '-I/home/stephan/Projects/constellation_asio_sd',
    '-I/home/stephan/Projects/constellation_asio_sd/subprojects/asio-1.28.1/include',
]
