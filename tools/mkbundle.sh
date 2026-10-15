#!/bin/sh
#
# mkbundle.sh - package a release as a git bundle for recipients who have
# no access to this repository's git server.
#
# Usage: tools/mkbundle.sh TO [FROM] [OUTDIR]
#
#   TO       release tag being delivered (e.g. v0.6)
#   FROM     tag the recipient already has (e.g. v0.5).  Omit it to create
#            a full bundle containing the complete history up to TO, for
#            first-time recipients.
#   OUTDIR   where to write the bundle (default: ../heliod-releases)
#
# Writes OUTDIR/heliod-TO.bundle (full) or OUTDIR/heliod-FROM-to-TO.bundle
# (incremental) and verifies it.  An incremental bundle can only be
# fetched into a repository that already contains FROM; git refuses to
# unpack it otherwise, so a stale recipient cannot silently end up with a
# broken tree.
#
# Recipient side:
#
#     git bundle verify heliod-v0.5-to-v0.6.bundle
#     git fetch heliod-v0.5-to-v0.6.bundle 'refs/tags/*:refs/tags/*'
#     git merge v0.6          # or: git checkout v0.6
#
#     git clone --branch v0.6 heliod-v0.6.bundle heliod   # first-time recipient
#
# The bundle carries the tag TO and every commit reachable from it that is
# not reachable from FROM, so recipients get real history they can merge
# or rebase their own changes onto.

set -eu

if [ $# -lt 1 ] || [ $# -gt 3 ]; then
    echo "usage: $0 TO [FROM] [OUTDIR]" >&2
    exit 2
fi

TO=$1
FROM=${2:-}
OUTDIR=${3:-$(git rev-parse --show-toplevel)/../heliod-releases}

cd "$(git rev-parse --show-toplevel)"
git rev-parse --verify --quiet "refs/tags/$TO" >/dev/null || { echo "$0: $TO is not a tag; releases are delivered from tags only" >&2; exit 1; }
if [ -n "$FROM" ]; then
    git rev-parse --verify --quiet "refs/tags/$FROM" >/dev/null || { echo "$0: $FROM is not a tag" >&2; exit 1; }
    git merge-base --is-ancestor "$FROM" "$TO" || { echo "$0: $FROM is not an ancestor of $TO" >&2; exit 1; }
fi

mkdir -p "$OUTDIR"
OUTDIR=$(cd "$OUTDIR" && pwd)

if [ -n "$FROM" ]; then
    BUNDLE="$OUTDIR/heliod-$FROM-to-$TO.bundle"
    git bundle create --quiet "$BUNDLE" "$FROM..$TO"
else
    BUNDLE="$OUTDIR/heliod-$TO.bundle"
    git bundle create --quiet "$BUNDLE" "$TO"
fi

git bundle verify --quiet "$BUNDLE"

if [ -n "$FROM" ]; then
    COMMITS=$(git rev-list --count "$FROM..$TO")
    RANGE="$FROM..$TO"
else
    COMMITS=$(git rev-list --count "$TO")
    RANGE="$TO (full history)"
fi
echo "$BUNDLE"
echo "  $RANGE: $COMMITS commits, $(du -h "$BUNDLE" | cut -f1); bundle verified"
