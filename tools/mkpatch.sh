#!/bin/sh
#
# mkpatch.sh - generate release patches between two tagged states.
#
# Usage: tools/mkpatch.sh FROM TO [OUTDIR]
#
#   FROM, TO   git tags (or any commit-ish) describing the start and end
#              states.  Both should be release tags; patches generated
#              from untagged commits cannot be reproduced later.
#   OUTDIR     where to write the patch (default: ../heliod-patches)
#
# Writes OUTDIR/heliod-FROM-to-TO.patch, then verifies that it applies
# cleanly to a pristine checkout of FROM.  Exits non-zero if it does not.
#
# Recipients apply the patch to an unmodified FROM tree with:
#
#     git apply heliod-FROM-to-TO.patch        (or: patch -p1 < ...)
#
# To let recipients who skipped a release catch up, generate a cumulative
# patch from the oldest base still in use as well as the incremental one,
# e.g. both v0.5..v0.6 and v0.4..v0.6.

set -eu

if [ $# -lt 2 ] || [ $# -gt 3 ]; then
    echo "usage: $0 FROM TO [OUTDIR]" >&2
    exit 2
fi

FROM=$1
TO=$2
OUTDIR=${3:-$(git rev-parse --show-toplevel)/../heliod-patches}

cd "$(git rev-parse --show-toplevel)"
git rev-parse --verify --quiet "$FROM^{commit}" >/dev/null || { echo "$0: unknown ref: $FROM" >&2; exit 1; }
git rev-parse --verify --quiet "$TO^{commit}" >/dev/null   || { echo "$0: unknown ref: $TO" >&2; exit 1; }

mkdir -p "$OUTDIR"
OUTDIR=$(cd "$OUTDIR" && pwd)
PATCH="$OUTDIR/heliod-$FROM-to-$TO.patch"

# Repository housekeeping files are not part of the delivered source.
git diff --binary "$FROM" "$TO" -- . ':!.gitignore' ':!.gitattributes' > "$PATCH"

if [ ! -s "$PATCH" ]; then
    echo "$0: no differences between $FROM and $TO" >&2
    rm -f "$PATCH"
    exit 1
fi

# Verify against a pristine FROM tree.
WT=$(mktemp -d "${TMPDIR:-/tmp}/mkpatch-verify.XXXXXX")
trap 'git worktree remove --force "$WT" 2>/dev/null; rm -rf "$WT"' EXIT
git worktree add --quiet --detach "$WT" "$FROM"
git -C "$WT" apply --check "$PATCH"

FILES=$(grep -c '^diff --git' "$PATCH")
STAT=$(git -C "$WT" apply --numstat "$PATCH" | awk '{a+=$1; d+=$2} END {printf "+%d -%d", a, d}')
SIZE=$(du -h "$PATCH" | cut -f1)
echo "$PATCH"
echo "  $FROM..$TO: $FILES files, $STAT, $SIZE; applies cleanly to pristine $FROM"
