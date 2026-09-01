"""What Godot's enums are, and whether L^ has any use for them.

A listing to decide from: which of them should become 02 の 19 章's enum^
and which are better as plain constants under the class. Writes
docs/godot-enums.md. Reads the same extension_api.json and the same
scripts/godot-classes.txt the binding generator reads, and asks the same
question about which classes are bound -- so "unreachable" here means
unreachable through the binding as it stands.

    python scripts/list-godot-enums.py
"""
import io
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from importlib import import_module

gen = import_module("gen-godot-api".replace("-", "_")) if False else None

# The generator's module name has hyphens, so it is read rather than imported.
_source = io.open(os.path.join(HERE, "gen-godot-api.py"), encoding="utf-8").read()
_namespace = {"__name__": "gen_godot_api", "__file__": os.path.join(HERE, "gen-godot-api.py")}
exec(compile(_source, "gen-godot-api.py", "exec"), _namespace)

API = _namespace["API"]
selected = _namespace["selected"]
DOC = os.path.join(os.path.dirname(HERE), "docs", "godot-enums.md")


def callable_method(method):
    """Whether the binding could reach this method at all."""
    return not (method.get("is_virtual") or method.get("is_vararg") or
                method.get("is_static"))


def main():
    with io.open(API, encoding="utf-8") as source:
        api = json.load(source)
    classes = {c["name"]: c for c in api["classes"]}
    wanted, singletons, dropped = selected(classes, api)

    # Every enum, by the spelling a signature uses for it.
    declared = {}   # "Class.Enum" or "Enum" -> row
    def declare(owner, enum, api_type):
        key = ("%s.%s" % (owner, enum["name"])) if owner else enum["name"]
        declared[key] = {
            "owner": owner or "",
            "name": enum["name"],
            "bitfield": bool(enum.get("is_bitfield")),
            "values": len(enum["values"]),
            "api_type": api_type,
            "arg": 0, "ret": 0, "prop": 0, "signal": 0,
            "arg_gd": 0, "ret_gd": 0,
        }

    for enum in api.get("global_enums", []):
        declare(None, enum, "core")
    for c in api["classes"]:
        for enum in c.get("enums", []):
            declare(c["name"], enum, c.get("api_type", "core"))

    def note(spelling, field):
        for prefix in ("enum::", "bitfield::"):
            if spelling.startswith(prefix):
                key = spelling[len(prefix):]
                if key in declared:
                    declared[key][field] += 1
                return

    # How the whole engine API names them: what a script can be handed and
    # what it can hand over. `*_gd` counts only what the binding can reach --
    # a virtual, a vararg or a static is not a method L^ calls.
    for c in api["classes"]:
        for method in c.get("methods", []):
            reachable = callable_method(method)
            for a in method.get("arguments", []):
                note(a["type"], "arg")
                if reachable:
                    note(a["type"], "arg_gd")
            answered = method.get("return_value", {}).get("type", "")
            if answered:
                note(answered, "ret")
                if reachable:
                    note(answered, "ret_gd")
        for described in c.get("properties", []):
            note(described.get("type", ""), "prop")
        for signal in c.get("signals", []):
            for a in signal.get("arguments", []):
                note(a["type"], "signal")

    out = []
    say = out.append
    say("# Godot の列挙体と、L^ に写すかどうか")
    say("")
    say("`scripts/list-godot-enums.py` が生成する。手で書き換えない。")
    say("")
    say("決めるための一覧である。02 の 19 章の `enum^` にするか、クラスの下の")
    say("定数にするか、そもそも写さないか。")
    say("")

    total = len(declared)
    unused = [k for k, r in declared.items()
              if r["arg"] + r["ret"] + r["prop"] + r["signal"] == 0]
    unreachable = [k for k, r in declared.items()
                   if r["owner"] and r["owner"] not in wanted]
    bits = [k for k, r in declared.items() if r["bitfield"]]
    say("- 宣言: **%d**（うちグローバル %d、bitfield %d）"
        % (total, sum(1 for r in declared.values() if not r["owner"]), len(bits)))
    say("- 値の総数: **%d**" % sum(r["values"] for r in declared.values()))
    say("- **どこからも名指されない: %d**" % len(unused))
    say("- 宣言したクラスがバインドされていない: %d" % len(unreachable))
    say("")
    say("## 印の読み方")
    say("")
    say("| 印 | 意味 |")
    say("| --- | --- |")
    say("| **未使用** | 引数・返り値・プロパティ・シグナルのどこにも現れない。"
        "C++ の内部でだけ意味を持つ定数であり、GDScript の書き手にも用が無い |")
    say("| **到達不可** | 宣言したクラスを `godot-classes.txt` が選んでいない。"
        "写しても名前を書く相手が居ない |")
    say("| **bitfield** | フラグの集合。OR で組むものなので `enum^` には合わない |")
    say("| **editor** | エディター API。配布ビルドには無い |")
    say("")
    say("`引数` / `返り` は**バインド済みで呼べるメソッド**での回数。"
        "括弧内はエンジン API 全体での回数（virtual・vararg・static を含む）。")
    say("")

    def rows_of(keys, note_text, title):
        say("## %s" % title)
        say("")
        say(note_text)
        say("")
        say("| 列挙体 | 値 | 引数 | 返り | プロパティ | シグナル | 印 |")
        say("| --- | --- | --- | --- | --- | --- | --- |")
        for key in keys:
            r = declared[key]
            marks = []
            if r["bitfield"]:
                marks.append("bitfield")
            if r["arg"] + r["ret"] + r["prop"] + r["signal"] == 0:
                marks.append("**未使用**")
            if r["owner"] and r["owner"] not in wanted:
                marks.append("**到達不可**")
            if r["api_type"] != "core":
                marks.append("editor")
            def both(now, all_of):
                return "%d" % now if now == all_of else "%d (%d)" % (now, all_of)
            say("| `%s` | %d | %s | %s | %d | %d | %s |"
                % (key, r["values"], both(r["arg_gd"], r["arg"]),
                   both(r["ret_gd"], r["ret"]), r["prop"], r["signal"],
                   " ".join(marks) or "—"))
        say("")

    ordered = sorted(declared, key=lambda k: (declared[k]["owner"], declared[k]["name"]))
    used = [k for k in ordered
            if declared[k]["arg"] + declared[k]["ret"] + declared[k]["prop"]
               + declared[k]["signal"] > 0]
    idle = [k for k in ordered if k not in set(used)]

    rows_of([k for k in used if not declared[k]["owner"]],
            "モジュールのもの。クラスに属さないので、写すならそのまま `godot.名前`。",
            "グローバル — 使われているもの")
    rows_of([k for k in used if declared[k]["owner"]],
            "クラスの下にあるもの。同じ綴りが別のクラスにもあるので、"
            "`enum^` にするなら名前を一意にする必要がある。",
            "クラス — 使われているもの")
    rows_of(idle,
            "エンジン API のどこにも現れない。GDScript から見ても値として"
            "受け取ることも渡すこともできない、C++ の内部のためだけの定数である。",
            "どこからも名指されないもの")

    while out and out[-1] == "":
        out.pop()
    with io.open(DOC, "w", encoding="utf-8", newline="\n") as target:
        target.write("\n".join(out) + "\n")
    print("%s: %d declarations, %d unused" % (DOC, total, len(unused)))


if __name__ == "__main__":
    main()
