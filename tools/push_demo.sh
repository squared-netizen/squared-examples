#!/usr/bin/env bash
# push_demo.sh: commit an example in the squared-examples repo, framework and
# all, and optionally push it.
#
#   tools/push_demo.sh --dry-run   show what would happen, change nothing
#   tools/push_demo.sh             commit locally, show the log
#   tools/push_demo.sh --push      the same, then push
#
# Options:
#   [DIR] | --demo DIR  the example's source (default: <examples>/<dir>, which
#                       is committed where it is; a project elsewhere is mirrored in)
#   --examples DIR      squared-examples repo (default: the repo this script sits in,
#                       else ~/sqsysroot/project/squared-examples)
#   --dir NAME          subdirectory in the repo (default demo)
#   --msg TEXT          commit subject
#   --package DIR       the squared resources repo, for the policy check
#                       (default ~/projects/squared-pg/resources/.squared)
#
# An example is a whole workspace: squared/ is committed with it, so a clone
# builds with nothing else. Build output and *.bak.* are never committed.
#
# Policy (MAINTAINING.md): framework code travels package -> example only.
# Before committing, the example's squared/ is compared with the package commit
# it names (<example>/SQUARED_REV). If they differ, nothing is committed: the
# change belongs in the package.
#
# Re-runnable: with no changes it commits nothing. Options are flags, not
# environment variables: Termux already sets PREFIX.
set -euo pipefail

DEMO=
here=$(cd "$(dirname "$0")/.." && pwd -P)
if [ -d "$here/.git" ] && git -C "$here" remote get-url origin 2>/dev/null | grep -q squared-examples; then
  EXAMPLES=$here
else
  EXAMPLES=$HOME/sqsysroot/project/squared-examples
fi
SUBDIR=demo
MSG=
EXAMPLES_REMOTE=squared-netizen/squared-examples
PKG_REPO=$HOME/projects/squared-pg/resources/.squared
TREE=packages/package.squared-core/tree
# Top-level .gitignore rules for squared-examples; they cover every example dir.
# squared/.vendored-rev was written by an earlier vendor_framework.sh; ignored.
IGNORE=('/*/build/' '/*/squared/build/release/' '/*/squared/build/debug/' '/*/squared/.vendored-rev' '*.bak.*')
# A rule from before examples carried their framework; removed if present.
RETIRED='/*/squared/'

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
    --package) need "$@"; PKG_REPO=$2; shift ;;
    -h | --help)
      sed -n '2,29p' "$0"
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
[ -n "$DEMO" ] || DEMO=$EXAMPLES/$SUBDIR

say() { printf '== %s\n' "$*"; }
# The revision a file names, or nothing (no error) if the file is absent.
read_rev() { if [ -f "$1" ]; then tr -d ' \t\r\n' <"$1"; fi; }
die() {
  printf 'stop: %s\n' "$*" >&2
  exit 1
}

say "checks"
case $SUBDIR in
  '' | /* | *..* | */*) die "--dir must be a single directory name, got '$SUBDIR'" ;;
esac
[ -d "$DEMO" ] || die "no project at $DEMO (pass --demo DIR)"
[ -d "$DEMO/sq_app" ] && [ -f "$DEMO/Makefile" ] || die "$DEMO doesn't look like a squared project (no sq_app/ or Makefile)"
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
[ -f "$SRC/squared/build/common.mk" ] ||
  die "$SRC has no framework (squared/build/common.mk missing); run tools/vendor_framework.sh $SUBDIR first"

# Which package commit the framework copy came from: the example's SQUARED_REV,
# or for a new example without one, the package's last commit. Either way the
# check below proves it, byte for byte.
git -C "$PKG_REPO" rev-parse --git-dir >/dev/null 2>&1 || die "no squared repo at $PKG_REPO for the policy check (pass --package DIR)"
FW_REV=$(read_rev "$SRC/SQUARED_REV")
[ -n "$FW_REV" ] || FW_REV=$(git -C "$PKG_REPO" rev-parse HEAD)
FW_REV=$(git -C "$PKG_REPO" rev-parse --verify -q "$FW_REV^{commit}") || die "package commit named by $SUBDIR/SQUARED_REV is not in $PKG_REPO"
FW_SHORT=${FW_REV:0:7}
echo "  source:       $SRC"
echo "  repo:         $EX -> $SUBDIR/$([ "$INPLACE" = 1 ] && echo ' (already there, commit only)')"
echo "  squared-core: package $FW_SHORT"

# Policy check: squared/ must be exactly the package at that commit.
TMP=$(mktemp -d "${TMPDIR:-/tmp}/sqpush.XXXXXX")
trap 'rm -rf "$TMP"' EXIT
git -C "$PKG_REPO" archive --format=tar "$FW_REV" "$TREE/squared" | tar -x -C "$TMP"
# Files of a framework tree, skipping only build output, the marker and backups.
fw_files() {
  (cd "$1" && find . \( -path ./build/release -o -path ./build/debug -o -path ./.vendored-rev \
    -o -name '*.bak.*' \) -prune -o -type f -print | LC_ALL=C sort)
}
fw_files "$TMP/$TREE/squared" >"$TMP/want"
fw_files "$SRC/squared" >"$TMP/have"
drift=$({
  LC_ALL=C comm -23 "$TMP/want" "$TMP/have" | sed 's|^\./|  missing   |'
  LC_ALL=C comm -13 "$TMP/want" "$TMP/have" | sed 's|^\./|  added     |'
  LC_ALL=C comm -12 "$TMP/want" "$TMP/have" | while IFS= read -r f; do
    cmp -s "$TMP/$TREE/squared/$f" "$SRC/squared/$f" || printf '  changed   %s\n' "${f#./}"
  done
} | head -8)
if [ -n "$drift" ]; then
  printf '%s\n' "$drift" >&2
  die "$SUBDIR/squared differs from package $FW_SHORT. Framework changes go in the package first, then tools/vendor_framework.sh $SUBDIR --latest (MAINTAINING.md). Nothing committed"
fi
echo "  policy:       $SUBDIR/squared matches package $FW_SHORT"
if [ "$DRY" = 1 ]; then echo "  dry run: nothing will change"; fi

cd "$EX"

say "squared-examples: .gitignore"
if grep -qxF -- "$RETIRED" .gitignore 2>/dev/null; then
  echo "  remove $RETIRED (examples now carry their framework)"
  if [ "$DRY" = 0 ]; then
    grep -vxF -- "$RETIRED" .gitignore >"$TMP/gitignore" || true
    cat "$TMP/gitignore" >.gitignore
  fi
fi
for p in "${IGNORE[@]}"; do
  if ! grep -qxF -- "$p" .gitignore 2>/dev/null; then
    echo "  add $p"
    if [ "$DRY" = 0 ]; then printf '%s\n' "$p" >>.gitignore; fi
  fi
done

# The files that belong in the example: everything except build output, .git
# and backups. Printed relative to the source, NUL-separated.
list_source() {
  (cd "$SRC" && find . \( -path ./build -o -path ./squared/build/release -o -path ./squared/build/debug \
    -o -path ./squared/.vendored-rev -o -name .git -o -name '*.bak.*' \) -prune -o ! -type d -print0)
}

n=$(list_source | tr -dc '\0' | wc -c | tr -d ' ')
if [ "$INPLACE" = 1 ]; then
  say "$SUBDIR/ is the project itself; nothing to copy ($n files)"
else
  say "mirror $n files into $SUBDIR/ (skipping build output, .git, *.bak.*)"
  if [ "$DRY" = 0 ]; then
    git rm -r -q --ignore-unmatch -- "$SUBDIR"
    list_source | while IFS= read -r -d '' f; do
      f=${f#./}
      mkdir -p "$DST/$(dirname "$f")"
      cp -P -p "$SRC/$f" "$DST/$f"
    done
  fi
fi

# Record the framework commit beside the example.
if [ "$(read_rev "$DST/SQUARED_REV")" != "$FW_REV" ]; then
  echo "  $SUBDIR/SQUARED_REV <- $FW_SHORT"
  [ "$DRY" = 1 ] || printf '%s\n' "$FW_REV" >"$DST/SQUARED_REV"
fi

excl=()
for p in "${IGNORE[@]}"; do excl+=(-x "$p"); done

say "commit"
if [ "$DRY" = 1 ]; then
  echo "  (dry run) would stage .gitignore and $SUBDIR/, then commit:"
  echo "  ${MSG:-$SUBDIR: update (squared-core @ $FW_SHORT)}"
  new=$(git status --porcelain --untracked-files=all -- "$SUBDIR/squared" 2>/dev/null | wc -l | tr -d ' ')
  [ "$new" = 0 ] || echo "  ($new framework files to add or change under $SUBDIR/squared)"
else
  # Anything tracked that the rules now exclude is untracked; it stays on disk.
  git ls-files -ci --exclude-standard "${excl[@]}" -z -- "$SUBDIR" | xargs -0 -r git rm --cached -q --
  git add -A -- .gitignore "$SUBDIR"
  bad=$(git diff --cached --name-only -- "$SUBDIR/build" "$SUBDIR/squared/build/release" "$SUBDIR/squared/build/debug" | head -5)
  [ -z "$bad" ] || die "build output is staged under $SUBDIR/ (e.g. $bad); nothing committed"
  if git diff --cached --quiet; then
    echo "  nothing changed since the last commit"
  else
    git diff --cached --stat | tail -1
    git commit -q -m "${MSG:-$SUBDIR: update (squared-core @ $FW_SHORT)}" \
      -m "Framework: squared-core at package commit $FW_REV, unmodified (checked by tools/push_demo.sh)."
    echo "  committed $(git rev-parse --short HEAD)"
  fi
  git log --oneline -3
fi

if [ "$PUSH" = 1 ]; then
  say "push"
  if [ "$DRY" = 0 ]; then
    git -C "$EX" push -u origin HEAD
  else
    echo "  (dry run) would push $EX"
  fi
else
  say "not pushed; rerun with --push when the log above looks right"
fi
