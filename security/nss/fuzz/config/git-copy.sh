#!/usr/bin/env bash

set -ex

if [ $# -lt 3 ]; then
  echo "Usage: $0 <repo> <branch> <directory>" 1>&2
  exit 2
fi

REPO="$1"
COMMIT="$2"
DIR="$3"

echo "Copy '$COMMIT' from '$REPO' to '$DIR'"
if [ -f "$DIR"/.git-copy ]; then
  CURRENT=$(cat "$DIR"/.git-copy)
  if [ $(echo -n "$COMMIT" | wc -c) != "40" ]; then
    # On the off chance that $COMMIT is a remote head.
    ACTUAL=$(git ls-remote "$REPO" "$COMMIT" | cut -c 1-40 -)
  else
    ACTUAL="$COMMIT"
  fi
  if [ "$CURRENT" = "$ACTUAL" ]; then
    echo "Up to date."
    exit
  fi
fi

rm -rf "$DIR"
git init -q "$DIR"

RETRIES=3
COUNT=1
while [ $COUNT -lt $RETRIES ]; do
  git -C "$DIR" fetch -q --depth=1 "$REPO" "$COMMIT"
  if [ $? -eq 0 ]; then
    COUNT=0
    break
  fi
  COUNT=$((COUNT+1))
done

git -C "$DIR" reset -q --hard FETCH_HEAD
git -C "$DIR" rev-parse --verify HEAD > "$DIR"/.git-copy
rm -rf "$DIR"/.git
