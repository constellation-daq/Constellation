# SPDX-FileCopyrightText: 2023 Stephan Lachnit <stephanlachnit@debian.org>
# SPDX-License-Identifier: EUPL-1.2

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
