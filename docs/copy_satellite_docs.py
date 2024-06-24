# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: EUPL-1.2

"""
Copy Satellite READMEs to documentation and convert them for MyST consumption
"""

import re
import yaml


def front_matter_convert_myst(string: str) -> str:
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
            converted_front_matter += yaml_data["subtitle"] + "\n"

        if "description" in yaml_data.keys():
            converted_front_matter += (
                ":::{card} {octicon}`code-square;1em;sd-text-info` Description\n"
            )
            converted_front_matter += yaml_data["description"]
            converted_front_matter += "\n:::\n"

        if "services" in yaml_data.keys():
            sv = yaml_data["services"]
            converted_front_matter += f"::::{{grid}} 1 {len(sv)} {len(sv)} {len(sv)}\n:gutter: 3\n:margin: 0\n\n"
            for service in sv:
                converted_front_matter += (
                    ":::{grid-item-card} {octicon}`repo-forked;1em;sd-text-info` "
                )
                converted_front_matter += service
                converted_front_matter += "\n:::\n"

            converted_front_matter += "\n::::\n"

        # treat other header fields and add them to a table
        string = converted_front_matter + string_after_yaml

    return string
