# Development Tools

In the following, some tools that come in handy for the development of satellites for Constellation of the framework itself
are described. These tools are pre-configured in the [satellite project templates](../tutorials/templates.md), and are also
used in the same or a very similar configuration in the main Constellation repository.

Their goal is to ensure a coherent code base in terms of style, naming conventions and best practices.
A detailed description of the formatting and naming conventions chosen for the main Constellation repository can be found in the [Framework Development Guide](/framework_reference/naming).

## Formatting & Linting for C++

The `clang-format` tool is used to apply the project's coding style convention to all C++ files of the code base.
The format is defined in the `.clang-format` file in the root directory of the satellite project template as well as the main repository and mostly follows the suggestions defined by the standard LLVM style with minor modifications.
Most notably are the consistent usage of four white space characters as indentation and the column limit of 125 characters.

The `clang-tidy` tool provides linting of the source code.
The tool tries to detect possible errors (and thus potential bugs), dangerous constructs (such as uninitialized variables) as well as stylistic errors.
In addition, it ensures proper usage of modern C++ standards.
The configuration used for the `clang-tidy` command can be found in the `.clang-tidy` file in the root directory of the satellite project template as well as the main repository.

These rules are applied automatically when committing code to the repository via the [`pre-commit` hooks](#using-pre-commit).
Alternatively, the tools can also be manually executed after configuring the build with either `meson` or `CMake` using the following commands:

```sh
ninja -C builddir/ clang-format
ninja -C builddir/ clang-tidy
```

## Using `pre-commit`

The Constellation repository uses [`pre-commit`](https://pre-commit.com/) hooks to enforce common formatting and naming conventions.
`pre-commit` is a package manager for hooks to be run before committing code changes to the repository.

It needs to be activated once for the cloned repository by running:

```sh
pre-commit install
```

Now, `pre-commit` will run the configured checks every time a new commit is created and an output similar to the following is produced:

```sh
$ git commit

check for merge conflicts................................................Passed
check that scripts with shebangs are executable..........................Passed
fix end of files.........................................................Passed
fix utf-8 byte order marker..............................................Passed
mixed line ending........................................................Passed
don`t commit to branch...................................................Failed
- hook id: no-commit-to-branch
- exit code: 1
trim trailing whitespace.................................................Passed
codespell................................................................Passed
markdownlint.............................................................Passed
reuse lint-file..........................................................Passed
```

Here, the creation of the commit has been prevented because one of the checks has failed - in this case the check to not commit to the `main` branch.
New changes resolving the reported issues have to be added to the commit diff before attempting to commit anew.

Sometimes it is necessary to temporarily skip these checks for a specific commit.
This can be achieved by using:

```sh
git commit --no-verify
```

By default, `pre-commit` will run its checks only on files that have been modified in a commit.
In order to run `pre-commit` on all files of the repository instead, this command can be used:

```sh
pre-commit run --all-files
```

## Software BOM with `reuse`

Constellation follows the [REUSE](https://reuse.software/) recommendations for accessible software licensing.
Every file of the repository clearly states its license along with the copyright statement in a machine-readable format, e.g. for a C++ source file

```cpp
/**
 * @file
 * @brief Description of the file content
 *
 * @copyright Copyright (c) 2025 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */
```

and the Python equivalent

```py
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""
```

The `reuse` tool can be used to check compliance with the REUSE recommendations with the command

```sh
reuse lint
```

This command is part of the pre-commit hooks. A software bill of material (BOM) on the SPDX format can be generated using

```sh
reuse spdx
```
