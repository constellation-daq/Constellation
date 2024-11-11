# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: EUPL-1.2

"""
Copy Satellite READMEs to documentation and convert them for MyST consumption
"""

import pathlib
import re
import yaml

import sphinx.util.logging

logger = sphinx.util.logging.getLogger(__name__)


def find_header_file(lang: str, directory: pathlib.Path) -> pathlib.Path | None:
    """
    Find the header file of a satellite in the specified directory following the 'NameSatellite.hpp' naming scheme.
    """
    if lang == "cxx":
        for file in directory.glob("*Satellite.hpp"):
            return file  # Return the first matching file (assuming only one matches)
    elif lang == "py":
        main_py_file = directory / "__main__.py"
        if main_py_file.exists():
            with open(main_py_file, "r") as file:
                content = file.read()
                # Search for an import statement that imports the desired class
                match = re.search(r"from\s+\.(\w+)\s+import\s+main", content)
                if match:
                    # Extract the module name from the import statement (e.g., 'Mariner' from 'from .Mariner import main')
                    py_file = directory / f"{match.group(1)}.py"

                    if py_file.exists():
                        return py_file
    return None


def extract_parent_classes(lang: str, header_path: pathlib.Path) -> list[str] | None:
    """
    Extract the parent classes from the C++ header file or Python definition.
    """
    with open(header_path, "r") as file:
        content = file.read()

        if lang == "cxx":
            # Regular expression to find the parent class in C++ inheritance declaration
            match = re.search(r"class\s+\w+\s*(?:final)?\s*:\s*public\s+((?:\w+::)*(\w*Satellite))\b", content)
            if match:
                return [match.group(2)]
        elif lang == "py":
            # Regex to match Python class inheritance (might be multiple)
            match = re.search(r"class\s+\w+\s*\((.+)\):", content)
            if match:
                # split multiple classes, remove whitespaces
                return match.group(1).replace(" ", "").split(",")
    return None


def convert_satellite_readme(lang: str, in_path: pathlib.Path, out_path: pathlib.Path) -> str:
    """
    Converts and copies a satellite README. The output is written to `<out_path>/<satellite_type>.md`.

    Args:
        in_path: Path to README with YAML front-matter.
        out_path: Path to directory where to write the converted <arkdown file.

    Returns:
        Satellite type (taken from the parent directory).
    """
    with in_path.open(mode="r", encoding="utf-8") as in_file:
        file_input = in_file.read()
        file_output = convert_front_matter(file_input)

        # Parse base class and append configuration parameters
        header_path = find_header_file(lang, in_path.parent)
        if header_path:
            logger.verbose(f"Satellite definition file: {header_path}")
            # Extract the parent class from the header file
            parent_classes = extract_parent_classes(lang, header_path)
            if parent_classes:
                logger.verbose(f"Appending parameters for parent classes: {parent_classes}")
                file_output += "\n```{include} _parameter_header.md\n```\n"
                file_output += append_content(lang, parent_classes)
            else:
                logger.warning(f"No parent classes found in {header_path}.")
        else:
            logger.warning(f"No satellite definition found in {in_path.parent}")

        (out_path / in_path.parent.name).with_suffix(".md").write_text(file_output)
        return in_path.parent.name


def append_content(lang: str, parent_classes: list[str]) -> str:
    """
    Append content to the README.md file based on the parent class.
    """
    # List of classes whose content should be appended in order
    if any(class_name in ["ReceiverSatellite", "TransmitterSatellite"] for class_name in parent_classes):
        parent_classes.append("Satellite")

    # Append content for each relevant class
    append = ""
    for class_name in parent_classes:
        content_file = "_" + lang + "_" + class_name + ".md"
        if content_file and (pathlib.Path("satellites") / content_file).exists():
            append += "\n```{include} " + str(content_file) + "\n```\n"
    return append


def convert_front_matter(string: str) -> str:
    """
    Converts YAML front-matter in a string from Markdown to plain text in Markdown.

    Args:
        string: String formatted in Markdown with YAML front-matter.

    Returns:
        String formatted in Markdown without YAML front-matter.
    """
    # extract yaml from string
    yaml_match = re.match(r"^---\n(.+?)\n---\n", string, flags=re.DOTALL)

    if yaml_match:
        raw_yaml = yaml_match.group(1)
        yaml_data = yaml.safe_load(raw_yaml)
        yaml_endpos = yaml_match.end(0)
        string_after_yaml = string[yaml_endpos:]

        converted_front_matter = "# " + yaml_data["title"] + " Satellite\n"

        if "subtitle" in yaml_data.keys():
            converted_front_matter += "\n" + yaml_data["subtitle"] + "\n"

        string = converted_front_matter + string_after_yaml

    return string
