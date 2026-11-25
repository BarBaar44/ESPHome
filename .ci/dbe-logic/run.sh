#!/usr/bin/env bash
# Host test for the DBE control logic. See harness.cpp for what it covers.
# Run from the repo root: .ci/dbe-logic/run.sh
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT

# Both firmware variants (v2 relay board, v3 P-FET board) carry the same
# control_fans lambda, except Min Fan Speed (v3 only, a setting instead of
# the fixed 25 %); test each so they cannot drift apart unnoticed.
cp "$here/harness.cpp" "$build/"
for fw in DBEmain.yml DBEmainV3.yml; do
  echo "== $fw"
  python3 "$here/extract.py" "$root/DBE/$fw" "$build/body.inc"
  # Min Fan Speed exists only in the v3 firmware.
  defs=""
  grep -q "min_fanspeed" "$build/body.inc" && defs="-DHAS_MIN_FANSPEED"
  g++ -std=c++17 -Wall -Werror $defs -o "$build/dbe_logic" "$build/harness.cpp"
  "$build/dbe_logic"
done
