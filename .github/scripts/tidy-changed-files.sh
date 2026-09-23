#!/usr/bin/env bash
#
# Run 'FslBuildCheck.py --tidy' on the C++ files that changed between a base commit and HEAD.
# Fails if clang-tidy reports warnings/errors for the changed files or if it applied any fixes.
#
# Usage: tidy-changed-files.sh <base-commit> <features>
#
set -uo pipefail

base="$1"
features="$2"

if [ -z "$base" ] || ! git cat-file -e "${base}^{commit}" 2>/dev/null; then
  echo "No usable base commit ('$base'), skipping clang-tidy."
  exit 0
fi

mapfile -t files < <(git diff --name-only --diff-filter=ACMR "$base" HEAD -- '*.cpp' '*.hpp' | grep -E '^(DemoApps|DemoFramework)/' || true)
if [ ${#files[@]} -eq 0 ]; then
  echo "No changed C++ files, skipping clang-tidy."
  exit 0
fi

echo "Running clang-tidy on ${#files[@]} changed file(s)"

# prepare.sh references variables that might not be set, so it can not be sourced with "set -u"
set +u
# shellcheck disable=SC1091
source ./prepare.sh
set -u

root=$(pwd)
log=$(mktemp)
failed=0

for f in "${files[@]}"; do
  echo "::group::clang-tidy $f"
  if ! python3 .Config/FslBuildCheck.py --noGitHash --UseFeatures "$features" --tidy --file "$root/$f" 2>&1 | tee "$log"; then
    echo "::error file=$f::FslBuildCheck.py --tidy failed"
    failed=1
  fi
  echo "::endgroup::"

  # Report the clang-tidy diagnostics for the file itself as GitHub annotations
  while IFS= read -r line; do
    if [[ "$line" =~ ^(.*/)?${f}:([0-9]+):([0-9]+):\ (warning|error):\ (.*)$ ]]; then
      echo "::error file=$f,line=${BASH_REMATCH[2]},col=${BASH_REMATCH[3]}::${BASH_REMATCH[5]}"
      failed=1
    fi
  done < "$log"
done

if ! git diff --quiet; then
  echo "::error::clang-tidy applied fixes, please run 'FslBuildCheck.py --tidy' locally and commit the result"
  git --no-pager diff
  failed=1
fi

exit $failed
