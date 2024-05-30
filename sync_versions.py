#!/usr/bin/env python3
import re
import fileinput
from datetime import date

setup_py_pattern = r"version *= *[\"'](\d+\.\d+\.\d+)['\"]"

files = [
    (
        "configure.ac",
        r"AC_INIT\(\[tscan\], \[(\d+\.\d+\.\d+)\], .*",
    ),
    (
        "codemeta.json",
        r'"version": "(\d+\.\d+\.\d+)"',
    ),
    (
        "webservice/setup.py",
        setup_py_pattern,
    ),
    (
        "setup.py",
        setup_py_pattern,
    ),
    (
        "README.md",
        r"tscan (\d+\.\d+\.\d+) \(c\) TiCC/ 1998 - \d+",
    ),
]

# read versions
found = {}
for filepath, pattern in files:
    with open(filepath) as f:
        lines = f.readlines()
        for lineno, line in enumerate(lines):
            match = re.search(pattern, line)
            if match:
                found[filepath] = (lineno, match[1])
                break
        else:
            print(f"No version found in {filepath}")
            quit(1)


highest_version = (0, 0, 0)
status = 0
for i, (_, version) in enumerate(found.values()):
    major, minor, patch = (int(part) for part in version.split("."))
    if i > 0 and (major, minor, patch) != highest_version:
        status = 1
    if (major, minor, patch) > highest_version:
        highest_version = (major, minor, patch)

target_version = ".".join(str(part) for part in highest_version)
if status == 0:
    print(f"Version is already synced at {target_version}")
    exit(0)

print(f"Updating everything to {target_version}")

for filepath, pattern in files:
    lineno, _ = found[filepath]
    with fileinput.input(files=(filepath), inplace=True) as f:
        for i, line in enumerate(f):
            # redirected back to file
            if i == lineno:
                if filepath == "README.md":
                    print(
                        f"tscan {target_version} (c) TiCC/ 1998 - {date.today().year}"
                    )
                else:
                    start, end = re.search(pattern, line).span(1)
                    print(line[:start] + target_version + line[end:], end="")
            else:
                print(line, end="")

exit(status)
