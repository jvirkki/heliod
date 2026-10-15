# Releasing Heliod

This document describes how Heliod releases are cut and delivered to
recipients who do not have access to the local project's git server.

## Branches

- **`master`** is the delivery line. Nothing is developed on it directly;
  it only ever advances by **squash-merging** a finished piece of work, so
  its history is one commit per completed ticket or feature. Every
  release is tagged on `master`, and every bundle or patch is generated
  from those tags. Recipients therefore see completed work only — never
  the intermediate commits made while getting there.
- **Day-to-day work happens on a working branch** — currently
  `protocol-modernization`. Commit freely there; push it to the private
  server as often as you like. That history stays private.
- When a piece of work is complete (built, tested, ChangeLog entry
  written), squash-merge the working branch into `master` as a single
  commit whose message describes the completed work (e.g. the ticket).

Because a squash commit has no merge parent, the working branch must be
re-based on `master` immediately after each squash merge; otherwise the
next squash merge tries to re-apply changes that have already landed.

Every delivery is an annotated tag (`v0.4`, `v0.5`, `v0.6`, ...) on
`master`. Tags are never moved and delivered artifacts are never
hand-edited: if the content changes, fix it on the working branch,
squash-merge again, and cut a new tag.

## Landing a completed piece of work

```bash
# On the working branch: work is built, tested, ChangeLog entry committed.
git checkout master
git merge --squash protocol-modernization
git commit            # one commit: "Ticket 15: gate ALPN h2 on io-model" ...
git push origin master

# Re-base the working branch on master so the next squash merge is clean.
git checkout protocol-modernization
git reset --hard master
git push --force origin protocol-modernization
```

A fresh branch per ticket (`git checkout -b ticket-15 master`), deleted
after its squash merge, works the same way and avoids the reset step.

## Cutting a release

```bash
# All work for vN has been squash-merged into master (above).
git checkout master
git tag -a vN -m "Heliod vN"
git push origin vN
# Produce the release artifacts from the tags (see below).
```

## Delivery: git bundles (primary)

`tools/mkbundle.sh` packages a tag as a git bundle — a self-contained
file carrying the tag and its history:

```bash
tools/mkbundle.sh vN            # full bundle, for first-time recipients
tools/mkbundle.sh vN vN-1       # incremental bundle, for recipients who
                                # already have the previous tag
```

Both bundles should be produced for every release and shipped to
recipients.

### Recipient side

First-time recipients clone the full bundle:

```bash
git clone --branch vN heliod-vN.bundle heliod
```

(`--branch` is required: a bundle carries only the tag, so plain `git clone`
leaves the working tree unpopulated.)

Existing recipients fetch the incremental bundle:

```bash
git bundle verify heliod-vN-1-to-vN.bundle
git fetch heliod-vN-1-to-vN.bundle 'refs/tags/*:refs/tags/*'
git merge vN          # or: git checkout vN
```

An incremental bundle can only be fetched into a repository that already
contains the `FROM` tag; git refuses to unpack it otherwise.

## Fallback: patches (no git on the recipient side)

For recipients without git, `tools/mkpatch.sh` generates plain diffs:

```bash
tools/mkpatch.sh vN-1 vN        # incremental patch
tools/mkpatch.sh v0.4 vN        # cumulative patch from the oldest
                                # supported base (currently v0.4)
```

Recipients apply a patch to a pristine tree of the `FROM` tag:

```bash
git apply heliod-vN-1-to-vN.patch     # or: patch -p1 < ...
```

Each generated patch is verified by the script: it is test-applied to a
pristine checkout of `FROM` before being reported as ready.

## History note

`v0.4` and `v0.5` were originally cut on separate branches
(`build-modernization` and `protocol-modernization`) and predate the
squash-merge rule, so the `v0.4..v0.5` range (and therefore the full
bundles) still contains their development history. The history was
re-parented when the repository was split so that `v0.3`, `v0.4` and
`v0.5` form one ancestry on `master`; from `v0.5` onward every commit on
`master` is a completed piece of work.
