#!/usr/bin/env bash
# vendor_framework.sh: copy the squared framework from the package into an
# example. The one direction framework code may travel: package -> example.
#
#   tools/vendor_framework.sh demo               the package commit in demo/SQUARED_REV
#   tools/vendor_framework.sh demo --latest      the package's last commit; records it
#   tools/vendor_framework.sh --all [--latest]   every example in the repo
#   tools/vendor_framework.sh demo --dry-run     say what would happen
#
# Options:
#   --package DIR   the squared resources repo
#                   (default ~/projects/squared-pg/resources/.squared)
#   --force         replace squared/ even if it differs from the package
#                   commit it came from (it is backed up first)
#
# Policy (see MAINTAINING.md): the package is the only place framework code is
# changed. So this tool
#   - takes committed package code only (git archive of a commit), never the
#     package's working tree: a fix must be committed to the package first;
#   - stops if the example's squared/ differs from the package commit it came
#     from (<example>/SQUARED_REV), because a change made there belongs in the
#     package (--force overrides, with a backup);
#   - stops if the framework holds a {{placeholder}}: copying is only the same
#     as generating while the framework needs no substitution.
# It also updates <example>/mk/pkg_squared_core.mk, which the package owns, and
# writes <example>/SQUARED_REV. Commit both, with squared/, in the example.
#
# Runs from anywhere: the repo is found from this script's own path.
set -euo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)
PKG_REPO=$HOME/projects/squared-pg/resources/.squared
TREE=packages/package.squared-core/tree
LATEST=0
ALL=0
DRY=0
FORCE=0
examples=()
need() { [ $# -ge 2 ] && [ -n "$2" ] || { echo "$1 needs a value" >&2; exit 2; }; }
while [ $# -gt 0 ]; do
  case $1 in
    --latest) LATEST=1 ;;
    --all) ALL=1 ;;
    --dry-run) DRY=1 ;;
    --force) FORCE=1 ;;
    --package) need "$@"; PKG_REPO=$2; shift ;;
    -h | --help) sed -n '2,28p' "$0"; exit 0 ;;
    -*) echo "unknown option: $1 (see --help)" >&2; exit 2 ;;
    *) examples+=("${1%/}") ;;
  esac
  shift
done
die() { printf 'stop: %s\n' "$*" >&2; exit 1; }
# The revision a file names, or nothing (no error) if the file is absent.
read_rev() { if [ -f "$1" ]; then tr -d ' \t\r\n' <"$1"; fi; }

git -C "$PKG_REPO" rev-parse --git-dir >/dev/null 2>&1 \
  || die "no squared repo at $PKG_REPO (pass --package DIR)"
git -C "$PKG_REPO" cat-file -e "HEAD:$TREE/squared" 2>/dev/null \
  || die "$PKG_REPO has no $TREE/squared at HEAD"

if [ "$ALL" = 1 ]; then
  for d in "$here"/*/; do
    [ -d "$d/sq_app" ] && [ -f "$d/Makefile" ] && examples+=("$(basename "$d")")
  done
fi
[ ${#examples[@]} -gt 0 ] || die "name an example (e.g. demo) or pass --all"

# Files of a framework tree, skipping only build output, the marker and backups.
fw_files() {
  (cd "$1" && find . \( -path ./build/release -o -path ./build/debug -o -path ./.vendored-rev \
    -o -name '*.bak.*' \) -prune -o -type f -print | LC_ALL=C sort)
}

STAMP=$(date +%Y%m%d-%H%M%S)-$$
TMP=$(mktemp -d "${TMPDIR:-/tmp}/sqvendor.XXXXXX")
trap 'rm -rf "$TMP"' EXIT

for ex in "${examples[@]}"; do
  dir=$here/$ex
  [ -d "$dir/sq_app" ] || die "$ex is not an example in $here (no sq_app/)"
  echo "== $ex"

  if [ "$LATEST" = 1 ]; then
    rev=$(git -C "$PKG_REPO" rev-parse HEAD)
    if [ -n "$(git -C "$PKG_REPO" status --porcelain -- "$TREE")" ]; then
      echo "  note: the package has uncommitted changes; they are not taken. Commit them first if they belong here."
    fi
  else
    [ -f "$dir/SQUARED_REV" ] || die "$ex has no SQUARED_REV; run with --latest to take the package's last commit and record it"
    want=$(tr -d ' \t\r\n' <"$dir/SQUARED_REV")
    [ -n "$want" ] || die "$ex/SQUARED_REV is empty"
    rev=$(git -C "$PKG_REPO" rev-parse --verify -q "$want^{commit}") \
      || die "commit $want (from $ex/SQUARED_REV) is not in $PKG_REPO; pull it, or use --latest"
  fi
  short=${rev:0:7}

  # Policy: the framework copy must be the package commit it came from. Any
  # difference is a change made in the example, and belongs in the package.
  if [ -d "$dir/squared" ]; then
    # It came from the commit in SQUARED_REV; an example without one (a fresh
    # workspace) is compared with the package's last commit.
    came_from=$(read_rev "$dir/SQUARED_REV")
    [ -n "$came_from" ] || came_from=$(git -C "$PKG_REPO" rev-parse HEAD)
    drift=""
    if ! git -C "$PKG_REPO" cat-file -e "$came_from^{commit}" 2>/dev/null; then
      drift="  ($ex/SQUARED_REV names ${came_from:0:7}, which is not in $PKG_REPO)"
    else
      rm -rf "$TMP/was"; mkdir -p "$TMP/was"
      git -C "$PKG_REPO" archive --format=tar "$came_from" "$TREE/squared" | tar -x -C "$TMP/was"
      fw_files "$TMP/was/$TREE/squared" >"$TMP/want"
      fw_files "$dir/squared" >"$TMP/have"
      drift=$({
        LC_ALL=C comm -23 "$TMP/want" "$TMP/have" | sed 's|^\./|  missing   |'
        LC_ALL=C comm -13 "$TMP/want" "$TMP/have" | sed 's|^\./|  added     |'
        LC_ALL=C comm -12 "$TMP/want" "$TMP/have" | while IFS= read -r f; do
          cmp -s "$TMP/was/$TREE/squared/$f" "$dir/squared/$f" || printf '  changed   %s\n' "${f#./}"
        done
      } | head -8)
    fi
    if [ -n "$drift" ]; then
      if [ "$FORCE" = 0 ]; then
        printf '%s\n' "$drift" >&2
        die "$ex/squared/ is not the package commit it came from. Framework changes go in the package first (MAINTAINING.md); move them there, or rerun with --force to discard them (a backup is kept)"
      fi
      echo "  --force: replacing $ex/squared/ despite local changes (backed up)"
    fi
  fi

  rm -rf "$TMP/x"; mkdir -p "$TMP/x"
  git -C "$PKG_REPO" archive --format=tar "$rev" "$TREE/squared" "$TREE/mk/pkg_squared_core.mk" | tar -x -C "$TMP/x"
  [ -f "$TMP/x/$TREE/squared/build/common.mk" ] || die "package commit $short has no squared/build/common.mk"
  placeholders=$(grep -rIl '{{[A-Za-z_][A-Za-z0-9_]*}}' "$TMP/x/$TREE" 2>/dev/null | sed "s|^$TMP/x/$TREE/||" | head -5 || true)
  [ -z "$placeholders" ] || die "package commit $short holds {{placeholders}} (e.g. $(echo $placeholders)); a plain copy would ship them unsubstituted. The framework must be regenerated through squared-pg instead"

  # Unchanged if it already is that commit: the drift check above passed
  # against SQUARED_REV, so equal revisions mean equal files.
  current=$(read_rev "$dir/SQUARED_REV")
  if [ "$current" = "$rev" ] && [ -f "$dir/squared/build/common.mk" ] && [ "$FORCE" = 0 ]; then
    echo "  squared/ already at $short"
  else
    echo "  squared/ <- package $short$([ -n "$current" ] && [ "$current" != "$rev" ] && echo " (was ${current:0:7})")"
    if [ "$DRY" = 0 ]; then
      if [ -e "$dir/squared" ]; then
        b=$HOME/.cache/squared-template-backups/$STAMP/$ex
        mkdir -p "$b"
        mv "$dir/squared" "$b/squared"
        echo "  previous squared/ moved to ${b#"$HOME"/}/squared"
      fi
      mv "$TMP/x/$TREE/squared" "$dir/squared"
    fi
  fi

  if [ -f "$TMP/x/$TREE/mk/pkg_squared_core.mk" ] && ! cmp -s "$TMP/x/$TREE/mk/pkg_squared_core.mk" "$dir/mk/pkg_squared_core.mk"; then
    echo "  mk/pkg_squared_core.mk <- package $short"
    if [ "$DRY" = 0 ]; then
      mkdir -p "$dir/mk"
      cp "$TMP/x/$TREE/mk/pkg_squared_core.mk" "$dir/mk/pkg_squared_core.mk"
    fi
  fi

  if [ "$(read_rev "$dir/SQUARED_REV")" != "$rev" ]; then
    echo "  SQUARED_REV <- $short"
    [ "$DRY" = 1 ] || printf '%s\n' "$rev" >"$dir/SQUARED_REV"
  fi
done

if [ "$DRY" = 1 ]; then
  echo "== dry run: nothing changed"
else
  echo "== next: commit the example (squared/, SQUARED_REV, mk/), e.g. tools/push_demo.sh --dir ${examples[0]}"
fi
