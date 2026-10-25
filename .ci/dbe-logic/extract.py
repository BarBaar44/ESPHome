"""Extract the control_fans lambda from DBE/DBEmain.yml for the host test.

Writes the lambda body to the path given as the second argument, with every
id(name) rewritten to G.name so it compiles against the mocks in harness.cpp.

Usage: python3 extract.py DBE/DBEmain.yml build/body.inc
"""
import re
import sys

import yaml


class Loader(yaml.SafeLoader):
    pass


# ESPHome tags we may meet; their value is all we need.
for tag in ("!lambda", "!secret", "!include"):
    Loader.add_constructor(tag, lambda loader, node: loader.construct_scalar(node))

config = yaml.load(open(sys.argv[1], encoding="utf-8"), Loader=Loader)
script = next(s for s in config["script"] if s["id"] == "control_fans")
body = next(step["lambda"] for step in script["then"] if "lambda" in step)
body = re.sub(r"id\((\w+)\)", r"G.\1", body)

with open(sys.argv[2], "w", encoding="utf-8") as out:
    out.write(body)
