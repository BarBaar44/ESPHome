#!/usr/bin/env bash
# Host test for the DBE control logic. See harness.cpp for what it covers.
# Run from the repo root: .ci/dbe-logic/run.sh
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT

# Both firmware variants (v2 relay board, v3 P-FET board) carry the same
# control_fans lambda; test each so they cannot drift apart unnoticed.
cp "$here/harness.cpp" "$build/"
for fw in DBEmain.yml DBEmainV3.yml; do
  echo "== $fw"
  python3 "$here/extract.py" "$root/DBE/$fw" "$build/body.inc"
  g++ -std=c++17 -Wall -Werror -o "$build/dbe_logic" "$build/harness.cpp"
  "$build/dbe_logic"
done
