#!/bin/bash
# inline_base.sh: replace FROM barelog:base with content of Dockerfile.base

df="$1"
base="${2:-Dockerfile.base}"

if [ -z "$df" ]; then
    echo "Usage: $0 <Dockerfile> [Dockerfile.base]"
    exit 1
fi

if [ ! -f "$df" ]; then
    echo "Error: $df not found"
    exit 1
fi

if [ ! -f "$base" ]; then
    echo "Error: $base not found"
    exit 1
fi

tmp=$(mktemp)

while IFS= read -r line; do
    if [[ "$line" == FROM\ barelog:base ]]; then
        cat "$base"
    else
        echo "$line"
    fi
done < "$df" > "$tmp"

mv "$tmp" "$df"
echo "Inlined $base into $df"
