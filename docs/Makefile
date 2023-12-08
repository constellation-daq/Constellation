# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

PYTHON     ?= python3
SPHINXOPTS ?=
SOURCEDIR  ?= source
BUILDDIR   ?= build

export PYTHONPATH=../src/

.PHONY: help
help:
	@$(PYTHON) -m sphinx -M help $(SOURCEDIR) $(BUILDDIR)

.PHONY: clean
clean:
	@$(PYTHON) -Bc "import shutil; shutil.rmtree('$(BUILDDIR)', ignore_errors=True)"

.PHONY: Makefile
%: Makefile
	@$(PYTHON) -m sphinx -M $@ $(SOURCEDIR) $(BUILDDIR) $(SPHINXOPTS) $(O)
