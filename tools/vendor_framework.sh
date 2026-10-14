#!/usr/bin/env bash
# vendor_framework.sh: put the squared framework into an example, at the commit
# the example records, so a fresh clone of squared-examples can build.
#
#   tools/vendor_framework.sh demo               the commit in demo/SQUARED_REV
#   tools/vendor_framework.sh demo --latest      the newest framework; records it
#   tools/vendor_framework.sh --all [--latest]   every example in the repo
#   tools/vendor_framework.sh demo --dry-run     say what would happen
#
# Options:
#   --latest         take the newest framework (your local package's HEAD, or
#                    GitHub's main) and write it to <example>/SQUARED_REV
#   --package DIR    the squared resources repo (default
#                    ~/projects/squared-pg/resources/.squared)
#   --offline        never fetch from GitHub
#
# Where the framework comes from, in order:
#   1. your local squared repo, if it has the commit (no network)
#   2. github.com/squared-netizen/squared, cached in ~/.cache/squared-vendor/
# Only files tracked at that commit are taken (git archive): no build output,
# no backups. They replace <example>/squared/ whole; the old one is moved to
# ~/.cache/squared-template-backups/<stamp>/ first. mk/pkg_squared_core.mk is
# updated too when it differs, since the package owns it.
#
# Runs from anywhere: the repo is found from this script's own path.
set -euo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)
PKG_REPO=$HOME/projects/squared-pg/resources/.squared
REMOTE=https://github.com/squared-netizen/squared.git
CACHE=$HOME/.cache/squared-vendor/squared.git
TREE=packages/package.squared-core/tree
LATEST=0
ALL=0
DRY=0
OFFLINE=0
examples=()
need() { [ $# -ge 2 ] && [ -n "$2" ] || { echo "$1 needs a value" >&2; exit 2; }; }
while [ $# -gt 0 ]; do
  case $1 in
    --latest) LATEST=1 ;;
    --all) ALL=1 ;;
    --dry-run) DRY=1 ;;
    --offline) OFFLINE=1 ;;
    --package) need "$@"; PKG_REPO=$2; shift ;;
    -h | --help) sed -n '2,25p' "$0"; exit 0 ;;
    -*) echo "unknown option: $1 (see --help)" >&2; exit 2 ;;
    *) examples+=("${1%/}") ;;
  esac
  shift
done
die() { printf 'stop: %s\n' "$*" >&2; exit 1; }

if [ "$ALL" = 1 ]; then
  for d in "$here"/*/; do
    [ -d "$d/sq_app" ] && [ -f "$d/Makefile" ] && examples+=("$(basename "$d")")
  done
fi
[ ${#examples[@]} -gt 0 ] || die "name an example (e.g. demo) or pass --all"

STAMP=$(date +%Y%m%d-%H%M%S)
TMP=$(mktemp -d "${TMPDIR:-/tmp}/sqvendor.XXXXXX")
trap 'rm -rf "$TMP"' EXIT

have_local=0
if git -C "$PKG_REPO" rev-parse --git-dir >/dev/null 2>&1 \
  && git -C "$PKG_REPO" cat-file -e "HEAD:$TREE/squared" 2>/dev/null; then
  have_local=1
fi

# The repo holding a commit: local first, then the GitHub cache (fetched once).
fetched=0
repo_for() { # repo_for <rev-or-empty-for-latest>
  local rev=$1
  if [ "$have_local" = 1 ]; then
    if [ -z "$rev" ] || git -C "$PKG_REPO" cat-file -e "$rev^{commit}" 2>/dev/null; then
      echo "$PKG_REPO"; return
    fi
  fi
  [ "$OFFLINE" = 0 ] || die "commit ${rev:-latest} is not in $PKG_REPO, and --offline was given"
  if [ "$fetched" = 0 ]; then
    if [ -d "$CACHE" ]; then
      echo "  fetching $REMOTE" >&2
      git -C "$CACHE" fetch -q origin '+refs/heads/*:refs/heads/*' >&2
    else
      echo "  cloning $REMOTE (once, into ${CACHE#"$HOME"/})" >&2
      mkdir -p "$(dirname "$CACHE")"
      git clone -q --bare --filter=blob:none "$REMOTE" "$CACHE" >&2
    fi
    fetched=1
  fi
  if [ -n "$rev" ]; then
    git -C "$CACHE" cat-file -e "$rev^{commit}" 2>/dev/null || die "commit $rev is not on GitHub either"
  fi
  echo "$CACHE"
}

for ex in "${examples[@]}"; do
  dir=$here/$ex
  [ -d "$dir/sq_app" ] || die "$ex is not an example in $here (no sq_app/)"
  echo "== $ex"

  if [ "$LATEST" = 1 ]; then
    repo=$(repo_for "")
    if [ "$repo" = "$PKG_REPO" ]; then rev=$(git -C "$repo" rev-parse HEAD); else rev=$(git -C "$repo" rev-parse main); fi
    if [ "$repo" = "$PKG_REPO" ] && [ -n "$(git -C "$PKG_REPO" status --porcelain -- "$TREE")" ]; then
      echo "  note: the package has uncommitted changes; vendoring its last commit, not the working tree"
    fi
  else
    [ -f "$dir/SQUARED_REV" ] || die "$ex has no SQUARED_REV; run with --latest to take the newest framework and record it"
    rev=$(tr -d ' \t\r\n' <"$dir/SQUARED_REV")
    [ -n "$rev" ] || die "$ex/SQUARED_REV is empty"
    repo=$(repo_for "$rev")
    rev=$(git -C "$repo" rev-parse "$rev^{commit}")
  fi
  short=${rev:0:7}
  where=$([ "$repo" = "$PKG_REPO" ] && echo "local package" || echo "GitHub")

  rm -rf "$TMP/x"; mkdir -p "$TMP/x"
  if [ "$DRY" = 0 ]; then
    git -C "$repo" archive --format=tar "$rev" "$TREE/squared" "$TREE/mk/pkg_squared_core.mk" | tar -x -C "$TMP/x"
    [ -f "$TMP/x/$TREE/squared/build/common.mk" ] || die "commit $short has no squared/build/common.mk; not a framework commit?"
  fi

  current=$(cat "$dir/squared/.vendored-rev" 2>/dev/null || true)
  if [ "$current" = "$rev" ] && [ -f "$dir/squared/build/common.mk" ]; then
    echo "  squared/ already at $short"
  else
    echo "  squared/ <- $short from $where$([ -n "$current" ] && echo " (was ${current:0:7})")"
    if [ "$DRY" = 0 ]; then
      printf '%s\n' "$rev" >"$TMP/x/$TREE/squared/.vendored-rev"
      if [ -e "$dir/squared" ]; then
        b=$HOME/.cache/squared-template-backups/$STAMP/$ex
        mkdir -p "$b"
        mv "$dir/squared" "$b/squared"
        echo "  old squared/ moved to ${b#"$HOME"/}/squared"
      fi
      mv "$TMP/x/$TREE/squared" "$dir/squared"
    fi
  fi

  if [ "$DRY" = 0 ] && [ -f "$TMP/x/$TREE/mk/pkg_squared_core.mk" ]; then
    if ! cmp -s "$TMP/x/$TREE/mk/pkg_squared_core.mk" "$dir/mk/pkg_squared_core.mk"; then
      mkdir -p "$dir/mk"
      cp "$TMP/x/$TREE/mk/pkg_squared_core.mk" "$dir/mk/pkg_squared_core.mk"
      echo "  mk/pkg_squared_core.mk updated (commit it with the example)"
    fi
  fi

  if [ "$LATEST" = 1 ] && [ "$(cat "$dir/SQUARED_REV" 2>/dev/null | tr -d ' \t\r\n')" != "$rev" ]; then
    echo "  SQUARED_REV <- $short (commit it with the example)"
    [ "$DRY" = 1 ] || printf '%s\n' "$rev" >"$dir/SQUARED_REV"
  fi
done
[ "$DRY" = 0 ] || echo "== dry run: nothing changed"
