#!/usr/bin/env bash
# push_demo.sh: copy an example project into the squared-examples repo as a
# subdirectory, leaving out squared/, build/, .git and *.bak.*, commit it, and
# optionally push squared-examples and the squared-pg resources repo.
#
#   bash push_demo.sh --dry-run   show what would happen, change nothing
#   bash push_demo.sh             copy and commit locally, show the log
#   bash push_demo.sh --push      the same, then push both remotes
#
# Options:
#   [DIR] | --demo DIR  source project (default ~/sqsysroot/project/demo,
#                       or <examples>/demo if that doesn't exist)
#   --examples DIR   squared-examples repo (default: the repo this script sits in,
#                    else ~/sqsysroot/project/squared-examples)
#   --dir NAME       subdirectory in the repo (default demo)
#   --msg TEXT       commit subject
# If --demo already is <examples>/<dir>, nothing is copied; it is only committed.
# The copy is a mirror: files deleted from the demo are deleted from <dir> too.
# Re-runnable: with no changes it commits nothing.
# Options are flags, not environment variables: Termux already sets PREFIX.
set -euo pipefail

DEMO=
# Inside squared-examples/tools/, the repo is the script's parent directory.
here=$(cd "$(dirname "$0")/.." && pwd -P)
if [ -d "$here/.git" ] && git -C "$here" remote get-url origin 2>/dev/null | grep -q squared-examples; then
  EXAMPLES=$here
else
  EXAMPLES=$HOME/sqsysroot/project/squared-examples
fi
SUBDIR=demo
MSG=
EXAMPLES_REMOTE=squared-netizen/squared-examples
PKG=$HOME/projects/squared-pg/resources/.squared/packages/package.squared-core
# Top-level .gitignore rules for squared-examples; they cover every example dir.
IGNORE=('/*/squared/' '/*/build/' '*.bak.*')

DRY=0
PUSH=0
need() { [ $# -ge 2 ] && [ -n "$2" ] || { echo "$1 needs a value" >&2; exit 2; }; }
while [ $# -gt 0 ]; do
  case $1 in
    --dry-run) DRY=1 ;;
    --push) PUSH=1 ;;
    --demo) need "$@"; DEMO=$2; shift ;;
    --examples) need "$@"; EXAMPLES=$2; shift ;;
    --dir) need "$@"; SUBDIR=$2; shift ;;
    --msg) need "$@"; MSG=$2; shift ;;
    --pkg) need "$@"; PKG=$2; shift ;;
    -h | --help)
      sed -n '2,20p' "$0"
      exit 0
      ;;
    -*)
      echo "unknown option: $1 (see --help)" >&2
      exit 2
      ;;
    *)
      [ -z "$DEMO" ] || { echo "two project paths given: $DEMO and $1" >&2; exit 2; }
      DEMO=$1
      ;;
  esac
  shift
done
if [ -z "$DEMO" ]; then
  DEMO=$HOME/sqsysroot/project/demo
  [ -d "$DEMO" ] || [ ! -d "$EXAMPLES/$SUBDIR" ] || DEMO=$EXAMPLES/$SUBDIR
fi

say() { printf '== %s\n' "$*"; }
die() {
  printf 'stop: %s\n' "$*" >&2
  exit 1
}

say "checks"
case $SUBDIR in
  '' | /* | *..* | */*) die "--dir must be a single directory name, got '$SUBDIR'" ;;
esac
[ -d "$DEMO" ] || die "no project at $DEMO (pass --demo DIR)"
[ -d "$DEMO/sq_app" ] || [ -f "$DEMO/Makefile" ] || die "$DEMO doesn't look like a squared project (no sq_app/ or Makefile); pass the project folder, e.g. $DEMO/demo"
git -C "$EXAMPLES" rev-parse --is-inside-work-tree >/dev/null 2>&1 ||
  die "$EXAMPLES is not a git repo (pass --examples DIR)"
EX=$(cd "$EXAMPLES" && pwd -P)
[ "$(git -C "$EX" rev-parse --show-toplevel)" = "$EX" ] || die "$EXAMPLES is not the top of its repo"
url=$(git -C "$EX" remote get-url origin 2>/dev/null || true)
case $url in
  *"$EXAMPLES_REMOTE"*) ;;
  *) die "origin of $EXAMPLES is '$url', not $EXAMPLES_REMOTE" ;;
esac
SRC=$(cd "$DEMO" && pwd -P)
DST=$EX/$SUBDIR
INPLACE=0
if [ "$SRC" = "$DST" ]; then
  INPLACE=1
else
  [ "$SRC" != "$EX" ] || die "that is the squared-examples repo itself; pass the project inside it, e.g. $EX/$SUBDIR"
  case $SRC/ in "$EX"/*) die "$DEMO is inside squared-examples but not at $SUBDIR/; pass --dir" ;; esac
  [ -z "$(git -C "$EX" status --porcelain)" ] ||
    die "squared-examples has uncommitted changes; commit or stash them first"
fi
# The framework commit this example builds against: the one vendor_framework.sh
# put in squared/, recorded as squared/.vendored-rev. Without that marker, fall
# back to the local package's HEAD, which must then be clean to mean anything.
PKG_TOP=$(git -C "$PKG" rev-parse --show-toplevel 2>/dev/null || true)
FW_REV=$(tr -d ' \t\r\n' <"$SRC/squared/.vendored-rev" 2>/dev/null || true)
if [ -n "$FW_REV" ]; then
  FW_FROM="squared/.vendored-rev"
else
  [ -n "$PKG_TOP" ] || die "$SRC/squared was not vendored by tools/vendor_framework.sh and there is no package at $PKG to name instead"
  [ -z "$(git -C "$PKG_TOP" status --porcelain -- "$PKG")" ] ||
    die "squared-core has uncommitted changes; the commit would name a package rev that doesn't match"
  FW_REV=$(git -C "$PKG_TOP" rev-parse HEAD)
  FW_FROM="package HEAD (squared/ not vendored by tools/vendor_framework.sh)"
fi
PKG_REV=${FW_REV:0:7}
echo "  source:       $SRC"
echo "  repo:         $EX -> $SUBDIR/$([ "$INPLACE" = 1 ] && echo ' (already there, commit only)')"
echo "  squared-core: $PKG_REV, from $FW_FROM"
if [ "$DRY" = 1 ]; then echo "  dry run: nothing will change"; fi

cd "$EX"

say "squared-examples: .gitignore"
for p in "${IGNORE[@]}"; do
  if ! grep -qxF -- "$p" .gitignore 2>/dev/null; then
    echo "  add $p"
    if [ "$DRY" = 0 ]; then printf '%s\n' "$p" >>.gitignore; fi
  fi
done

# The files that belong in the example: everything except squared/, build/,
# .git and backups. Printed relative to the source, NUL-separated.
list_source() {
  (cd "$SRC" && find . \( -path ./squared -o -path ./build -o -name .git -o -name '*.bak.*' \) -prune \
    -o ! -type d -print0)
}

n=$(list_source | tr -dc '\0' | wc -c | tr -d ' ')
if [ "$INPLACE" = 1 ]; then
  say "$SUBDIR/ is the project itself; nothing to copy ($n files)"
else
  say "mirror $n files into $SUBDIR/ (skipping squared/, build/, .git, *.bak.*)"
  if [ "$DRY" = 0 ]; then
    # Drop the tracked copy (git history keeps it), then lay down the current one.
    git rm -r -q --ignore-unmatch -- "$SUBDIR"
    list_source | while IFS= read -r -d '' f; do
      f=${f#./}
      mkdir -p "$DST/$(dirname "$f")"
      cp -P -p "$SRC/$f" "$DST/$f"
    done
  fi
fi

# Anything still tracked under <dir> that the ignore rules now exclude
# (only possible in place) is untracked here; it stays on disk.
tracked_ignored=$(git ls-files -ci --exclude-standard -x '/*/squared/' -x '/*/build/' -x '*.bak.*' -- "$SUBDIR")
if [ -n "$tracked_ignored" ]; then
  echo "  untracking (kept on disk), by dir:"
  printf '%s\n' "$tracked_ignored" | cut -d/ -f1-2 | sort | uniq -c
  if [ "$DRY" = 0 ]; then
    git ls-files -ci --exclude-standard -x '/*/squared/' -x '/*/build/' -x '*.bak.*' -z -- "$SUBDIR" |
      xargs -0 git rm --cached -q --
  fi
fi

# Record it beside the example, so tools/vendor_framework.sh can rebuild
# squared/ from a fresh clone.
if [ "$(tr -d ' \t\r\n' <"$DST/SQUARED_REV" 2>/dev/null || true)" != "$FW_REV" ]; then
  echo "  $SUBDIR/SQUARED_REV <- $PKG_REV"
  [ "$DRY" = 1 ] || printf '%s\n' "$FW_REV" >"$DST/SQUARED_REV"
fi

say "commit"
if [ "$DRY" = 1 ]; then
  echo "  (dry run) would stage .gitignore and $SUBDIR/, then commit:"
  echo "  ${MSG:-$SUBDIR: update (squared-core @ $PKG_REV)}"
else
  git add -A -- .gitignore "$SUBDIR"
  bad=$(git ls-files -- "$SUBDIR/squared" "$SUBDIR/build" | head -5)
  [ -z "$bad" ] || die "squared/ or build/ is staged under $SUBDIR/ (e.g. $bad); nothing committed"
  if git diff --cached --quiet; then
    echo "  nothing changed since the last commit"
  else
    git status --short -- .gitignore "$SUBDIR" | head -40
    git diff --cached --stat | tail -1
    git commit -q -m "${MSG:-$SUBDIR: update (squared-core @ $PKG_REV)}" \
      -m "Framework: squared-core @ $PKG_REV. squared/ is not tracked; restore it with tools/vendor_framework.sh $SUBDIR."
    echo "  committed $(git rev-parse --short HEAD)"
  fi
  git log --oneline -3
fi

if [ "$PUSH" = 1 ]; then
  say "push"
  if [ "$DRY" = 0 ]; then
    git -C "$EX" push -u origin HEAD
    if [ -n "$PKG_TOP" ]; then git -C "$PKG_TOP" push; fi
  else
    echo "  (dry run) would push $EX${PKG_TOP:+ and $PKG_TOP}"
  fi
else
  say "not pushed; rerun with --push when the log above looks right"
fi
