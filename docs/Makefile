# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

PYTHON     ?= python3
SPHINXOPTS ?=
SOURCEDIR  ?= .
BUILDDIR   ?= build

export PYTHONPATH=../python/

.PHONY: help
help:
	@$(PYTHON) -m sphinx -M help $(SOURCEDIR) $(BUILDDIR)

.PHONY: clean
clean:
	@$(PYTHON) -Bc "import shutil; shutil.rmtree('$(BUILDDIR)', ignore_errors=True)"

.PHONY: doxygen
doxygen:
	doxygen

.PHONY: apidoc
apidoc:
	@$(PYTHON) -m sphinx.ext.apidoc -e -T $(O) -o $(SOURCEDIR)/framework_reference/python/core $(PYTHONPATH)/constellation

.PHONY: livethml
livehtml:
	@$(PYTHON) -m sphinx_autobuild --ignore $(SOURCEDIR)/satellites $(SOURCEDIR) $(BUILDDIR) $(SPHINXOPTS) $(O)

.PHONY: Makefile
%: Makefile
	@$(PYTHON) -m sphinx -M $@ $(SOURCEDIR) $(BUILDDIR) $(SPHINXOPTS) $(O)
