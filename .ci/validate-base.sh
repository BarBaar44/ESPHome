#!/usr/bin/env bash
# Validates the real device configs in base/ with `esphome config`.
# Run from the repo root: .ci/validate-base.sh [base/<device>.yaml ...]
# Without arguments it validates every base/*.yaml.
#
# Two things make this work without the real HA dashboard:
#  1. secrets.yaml is a generated stub. Every !secret used in base/ gets a
#     dummy value: a valid 32 byte base64 key for names ending in _key,
#     a dummy password for everything else. Nothing real is ever used.
#  2. Package URLs pointing at this GitHub repo are rewritten to this
#     checkout (file://), so a PR is validated against its own package
#     files, not against what is on main. It clones the committed HEAD,
#     so commit local changes before running it on your own machine.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# Work on a copy, so base/ in the checkout is never touched.
cp -r "$root/base" "$work/base"

key="$(python3 -c 'import base64; print(base64.b64encode(b"0" * 32).decode())')"
grep -rhoE '!secret [A-Za-z0-9_-]+' "$root"/base/*.yaml | sort -u |
  awk -v k="$key" '{ v = ($2 ~ /_key$/) ? k : "ci-dummy-password"; print $2 ": \"" v "\"" }' \
  > "$work/base/secrets.yaml"

if [ "$#" -eq 0 ]; then
  set -- "$root"/base/*.yaml
fi

failed=()
for f in "$@"; do
  name="$(basename "$f")"
  [ "$name" = secrets.yaml ] && continue
  sed -i "s#url: https://github.com/BarBaar44/ESPHome#url: file://$root#I" "$work/base/$name"
  echo "::group::$name"
  if (cd "$work/base" && esphome config "$name" > "$work/$name.log" 2>&1); then
    echo "OK $name"
  else
    cat "$work/$name.log"
    failed+=("$name")
  fi
  echo "::endgroup::"
done

if [ "${#failed[@]}" -gt 0 ]; then
  echo "Failed: ${failed[*]}"
  exit 1
fi
