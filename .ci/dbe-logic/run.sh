#!/usr/bin/env bash
# Host test for the DBE control logic. See harness.cpp for what it covers.
# Run from the repo root: .ci/dbe-logic/run.sh
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT

python3 "$here/extract.py" "$root/DBE/DBEmain.yml" "$build/body.inc"
cp "$here/harness.cpp" "$build/"
g++ -std=c++17 -Wall -Werror -o "$build/dbe_logic" "$build/harness.cpp"
"$build/dbe_logic"
