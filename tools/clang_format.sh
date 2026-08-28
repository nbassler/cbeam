#!/usr/bin/env bash
# Format the C++ sources, or check them without touching anything.
#
# This is the single definition of which files are formatted and how they are
# invoked: the CI `format` job runs `tools/clang_format.sh --check`, so what
# passes here passes there. Style itself lives in .clang-format.
#
#   tools/clang_format.sh            reformat in place
#   tools/clang_format.sh --check    exit non-zero if anything is out of true
#
# Override the binary with CLANG_FORMAT=clang-format-19 if the default name is
# a different version -- clang-format output shifts between major versions and
# CI pins to whatever debian:trixie ships.

set -euo pipefail

cd "$(dirname "$0")/.."

CLANG_FORMAT=${CLANG_FORMAT:-clang-format}

if ! command -v "$CLANG_FORMAT" >/dev/null 2>&1; then
  echo "$CLANG_FORMAT not found; install clang-format or set CLANG_FORMAT" >&2
  exit 1
fi

files=(src/*.cpp src/*.h tests/*.cpp)

if [[ ${1:-} == --check ]]; then
  "$CLANG_FORMAT" --version
  exec "$CLANG_FORMAT" --dry-run --Werror "${files[@]}"
fi

if [[ -n ${1:-} ]]; then
  echo "usage: $0 [--check]" >&2
  exit 2
fi

"$CLANG_FORMAT" -i "${files[@]}"
echo "formatted ${#files[@]} files"
