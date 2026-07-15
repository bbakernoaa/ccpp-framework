#!/usr/bin/env python3
"""Standalone driver for emitting C++ framework artifacts (group shims +
suite factory) from suite XML files.

This script is invoked by CMake (via ccpp_capgen_cpp() function) when
CCPP_CPP_FRAMEWORK=ON. It reads one or more suite XML files, extracts
group names, and emits:
  - One Fortran bind(C) shim .F90 file per suite
  - One C++ suite factory .cpp file per suite

Usage:
    python3 emit_cpp.py --suites suite1.xml,suite2.xml --output-root /path/to/output

The generated file list is printed to stdout (semicolon-separated) for
CMake to consume via execute_process(OUTPUT_VARIABLE ...).
"""

import argparse
import os
import sys
import xml.etree.ElementTree as ET
from typing import List, Tuple


def parse_suite_xml_groups(suite_file: str) -> Tuple[str, List[str]]:
    """Parse a suite XML file and return (suite_name, [group_names]).

    Parameters
    ----------
    suite_file : str
        Path to a suite definition XML file.

    Returns
    -------
    tuple of (str, list of str)
        Suite name and ordered list of group names.
    """
    tree = ET.parse(suite_file)
    root = tree.getroot()
    suite_name = root.get("name", "")
    group_names = []
    for group_elem in root.findall("group"):
        name = group_elem.get("name", "")
        if name:
            group_names.append(name)
    return suite_name, group_names


def main():
    parser = argparse.ArgumentParser(
        description="Emit C++ framework artifacts from suite XML files.")
    parser.add_argument(
        "--suites", required=True,
        help="Comma-separated list of suite XML file paths")
    parser.add_argument(
        "--output-root", required=True,
        help="Directory where generated files are written")
    args = parser.parse_args()

    suite_files = [f.strip() for f in args.suites.split(",") if f.strip()]
    output_root = args.output_root
    os.makedirs(output_root, exist_ok=True)

    # Add the capgen directory to sys.path so generator modules are importable
    script_dir = os.path.dirname(os.path.abspath(__file__))
    if script_dir not in sys.path:
        sys.path.insert(0, script_dir)

    from generator.group_shim_emitter import emit_group_shims
    from generator.cpp_suite_factory_emitter import emit_suite_factory_with_shims

    generated_files: List[str] = []

    for suite_file in suite_files:
        if not os.path.isfile(suite_file):
            print("WARNING: Suite file '{}' not found, skipping".format(suite_file),
                  file=sys.stderr)
            continue

        suite_name, group_names = parse_suite_xml_groups(suite_file)
        if not suite_name or not group_names:
            print("WARNING: Could not parse suite/groups from '{}', skipping".format(
                suite_file), file=sys.stderr)
            continue

        # Emit Fortran bind(C) shims
        shim_path = emit_group_shims(
            suite_name=suite_name,
            group_names=group_names,
            output_dir=output_root,
        )
        generated_files.append(shim_path)

        # Emit C++ suite factory
        factory_path = emit_suite_factory_with_shims(
            suite_name=suite_name,
            group_names=group_names,
            output_dir=output_root,
        )
        generated_files.append(factory_path)

        print("Emitted C++ artifacts for suite '{}': {} groups, {} phases each".format(
            suite_name, len(group_names), 5), file=sys.stderr)

    # Print semicolon-separated file list to stdout for CMake
    print(";".join(generated_files))


if __name__ == "__main__":
    main()
