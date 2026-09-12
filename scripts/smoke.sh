#!/usr/bin/env bash
# L^ (lhat) -- whether what was just built is a library the engine loads and
# runs, rather than one that merely compiled.
#
# That distinction is the whole reason this exists. Everything the extension
# does happens inside a library Godot opens at run time, so a tree that
# builds can still leave every script unattached -- one registration refused
# is enough, and a signature with a hat missing did exactly that once. The
# engine says so only when it is running.
#
#     scripts/smoke.sh /path/to/godot
#
# The projects are copied under build/smoke and run there: a run writes
# .godot/, which belongs to nobody's tree, and the editor is what writes it.
# A game run without one still loads the library and attaches no scripts at
# all -- which looks clean and proves nothing, so the editor goes first.

set -euo pipefail

if [ $# -lt 1 ]; then
    echo "usage: $0 <godot>" >&2
    exit 2
fi
godot=$1
root=$(cd "$(dirname "$0")/.." && pwd)

if [ ! -d "$root/demo/bin" ] || [ -z "$(ls -A "$root/demo/bin" 2>/dev/null)" ]; then
    echo "nothing in demo/bin -- build first" >&2
    exit 2
fi

# What says the library is there but is not being used. An engine ERROR is
# not on the list: squash leaks an Ogg stream at exit and always has, and a
# check that fails on every line the engine dislikes would be turned off
# within the week. These are the ones that mean this extension.
bad='Cant open dynamic library|Can.t open dynamic library|No loader found for resource'
bad="$bad|Attempt to connect nonexistent signal|out of memory making the program"
bad="$bad|error:|GDExtension entry point|Error loading extension"

work="$root/build/smoke"
rm -rf "$work"
mkdir -p "$work"
failed=0

check() {
    local name=$1 log=$2
    if grep -nE "$bad" "$log"; then
        echo "FAIL $name: the lines above" >&2
        failed=1
    fi
}

for name in demo dodge_the_creeps squash_the_creeps; do
    case $name in
        demo) from="$root/demo" ;;
        *)    from="$root/demo-projects/$name" ;;
    esac
    copy="$work/$name"
    cp -r "$from" "$copy"
    rm -rf "$copy/.godot"
    mkdir -p "$copy/bin"
    rm -f "$copy"/bin/*
    cp "$root"/demo/bin/* "$copy/bin/" 2>/dev/null || true
    cp "$root/demo/lhat.gdextension" "$copy/"

    echo "== $name: importing =="
    "$godot" --headless --editor --path "$copy" --quit-after 200 \
        > "$work/$name.import.log" 2>&1 || true
    check "$name (import)" "$work/$name.import.log"

    echo "== $name: running =="
    "$godot" --headless --path "$copy" --quit-after 300 \
        > "$work/$name.run.log" 2>&1 || true
    check "$name (run)" "$work/$name.run.log"
done

# One positive answer, so that a run which printed nothing at all is not
# mistaken for a clean one. demo's main.gd asks the extension to check, run
# and call a unit, and reads a value back out of a .tres it wrote.
if ! grep -q "from the .tres: a lamp costs 45" "$work/demo.run.log"; then
    echo "FAIL demo: the .tres round trip said nothing" >&2
    sed -n '1,40p' "$work/demo.run.log" >&2
    failed=1
fi

if [ "$failed" -ne 0 ]; then
    exit 1
fi
echo "smoke: the extension loads, and the three projects run"
