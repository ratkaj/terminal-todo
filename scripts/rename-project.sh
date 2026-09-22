#!/bin/sh
# Rename the template's "skeleton" placeholder to a real project name.
# Usage: scripts/rename-project.sh <newname>   (lowercase, [a-z0-9_] only)
set -e
new="$1"
case "$new" in
	""|*[!a-z0-9_]*) echo "usage: $0 <lowercase_name>" >&2; exit 1 ;;
esac
upper=$(echo "$new" | tr a-z A-Z)
cd "$(dirname "$0")/.."
files=$(grep -rlE 'skeleton|SKELETON' --exclude-dir=.git \
	--exclude=unity.c --exclude=rename-project.sh .)
for f in $files; do
	sed -i "s/skeleton/$new/g; s/SKELETON/$upper/g" "$f"
done
echo "Renamed to '$new'. Run: autoreconf -i && ./configure --enable-tests"
