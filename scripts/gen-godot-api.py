"""L^ (lhat) -- generate the bound engine methods (src/lhat_godot_api.gen.cpp).

05 の 8.7: the checker has to be told what a host registered, and the engine
has to be told which method a call means. Both are settled here, ahead of
time, out of extension_api.json -- the file godot-cpp generates its own
bindings from.

8.8改 lets a registered type say what it is under, so what this writes is the
engine's tree: one host type per class, declared under the one it inherits
from, with every method a member of the class that declares it.

It has to be ahead of time. classdb_get_method_bind wants the compatibility
hash of the method, and no run-time engine call hands one out: what
ClassDB.class_get_method_list answers is a MethodInfo, which carries a name,
arguments, flags and an id, and no hash at all. So the hashes are read from
the same json godot-cpp reads, which is pinned to the godot-cpp the extension
is built against; hash_compatibility is what carries them into a later engine.

    python scripts/gen-godot-api.py

Reads scripts/godot-classes.txt for which classes to bind and
build/godot-cpp/gdextension/extension_api.json for what they are. What it
writes is committed: this is not part of the build, so a checkout needs
neither python nor an engine to compile.
"""

import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

API = os.path.join(ROOT, "build", "godot-cpp", "gdextension",
                   "extension_api.json")
CLASSES = os.path.join(HERE, "godot-classes.txt")
OUT = os.path.join(ROOT, "src", "lhat_godot_api.gen.cpp")
LH_DIR = os.path.join(ROOT, "demo", "lhat", "Godot")
DOCS_OUT = os.path.join(ROOT, "docs", "godot-classes.md")

# 05 の 8.9: the value types the module registers, whose bytes are the engine's
# own layout -- so an argument of one of these reaches ptrcall without being
# converted at all. The name on the right is the Variant::Type its tag is kept
# under (lhat_godot_module.h's value_tags).
HOSTVALUE = {
    "Vector2": "VECTOR2", "Vector2i": "VECTOR2I",
    "Vector3": "VECTOR3", "Vector3i": "VECTOR3I",
    "Vector4": "VECTOR4", "Vector4i": "VECTOR4I",
    "Color": "COLOR", "Quaternion": "QUATERNION",
    "Rect2": "RECT2", "Rect2i": "RECT2I",
    "AABB": "AABB", "Plane": "PLANE",
    "Transform2D": "TRANSFORM2D", "Basis": "BASIS",
    "Transform3D": "TRANSFORM3D", "Projection": "PROJECTION",
    "RID": "RID",
}

# 05 の 8.8: the handles, which hold a pointer rather than bytes. The pointer
# is the engine's own object, so these cross as they stand as well
# (lhat_godot_handles.h, lhat_godot_packed.h).
HOSTDATA = {
    "Callable": "CALLABLE", "Signal": "SIGNAL",
    "PackedByteArray": "PACKED_BYTE_ARRAY",
    "PackedInt32Array": "PACKED_INT32_ARRAY",
    "PackedInt64Array": "PACKED_INT64_ARRAY",
    "PackedFloat32Array": "PACKED_FLOAT32_ARRAY",
    "PackedFloat64Array": "PACKED_FLOAT64_ARRAY",
    "PackedStringArray": "PACKED_STRING_ARRAY",
    "PackedVector2Array": "PACKED_VECTOR2_ARRAY",
    "PackedVector3Array": "PACKED_VECTOR3_ARRAY",
    "PackedVector4Array": "PACKED_VECTOR4_ARRAY",
    "PackedColorArray": "PACKED_COLOR_ARRAY",
}

# The kinds whose argument is built rather than written into a machine word,
# and how many of them one signature may want (lhat_godot_module.h's
# LHAT_GD_MAX_BOXED). A method wanting none is handed no frame for them.
BOXED = {"LHAT_GD_STRING", "LHAT_GD_STRINGNAME", "LHAT_GD_NODEPATH",
         "LHAT_GD_VARIANT"}
MAX_BOXED = 4

# A string of the engine's is three types and one L^ type: what a script
# writes is text, and which flavour the engine wanted is the boundary's
# business rather than the writer's. Which one still has to be carried --
# a ptrcall reads the bytes as the type that was declared.
STRINGS = {
    "String": "LHAT_GD_STRING",
    "StringName": "LHAT_GD_STRINGNAME",
    "NodePath": "LHAT_GD_NODEPATH",
}


def kind_of(spelling, classes, wanted):
    """(kind expression, L^ type) for one engine type, or None when unbound."""
    # 02 の 14.8: one number^ over integers and reals, so an int, a float and
    # every enum are one type to a script. Which the engine wanted is still
    # carried, since ptrcall is handed a different width for each.
    if spelling.startswith("enum::") or spelling.startswith("bitfield::"):
        return "LHAT_GD_INT", "number^"
    if spelling == "int":
        return "LHAT_GD_INT", "number^"
    if spelling == "float":
        return "LHAT_GD_FLOAT", "number^"
    if spelling == "bool":
        return "LHAT_GD_BOOL", "bool^"
    if spelling in STRINGS:
        return STRINGS[spelling], "string^"
    if spelling == "Variant":
        return "LHAT_GD_VARIANT", "any^"
    if spelling in HOSTVALUE:
        return ("LHAT_GD_HOSTVALUE + Variant::" + HOSTVALUE[spelling],
                "godot." + spelling)
    if spelling in HOSTDATA:
        return ("LHAT_GD_HOSTDATA + Variant::" + HOSTDATA[spelling],
                "godot." + spelling)
    if spelling in classes:
        # 8.8改: the class itself where it was registered, and its nearest
        # registered ancestor otherwise -- which is also the tag a value of
        # it crosses with, so the two answers never disagree.
        for step in reversed(chain(spelling, classes)):
            if step in wanted:
                return "LHAT_GD_OBJECT", "godot." + step
        return "LHAT_GD_OBJECT", "godot.Object"
    return None  # Array, Dictionary, typedarray:: -- nothing stands for these


def chain(name, classes):
    walk = []
    while name:
        walk.append(name)
        name = classes[name].get("inherits")
    return walk[::-1]


# What Godot's own "create a new script" dialog offers as a parent, which is
# CreateDialog::_should_hide_type read the other way round: the core API, able
# to be instantiated, exposed to scripting, and not one of the two names that
# dialog blacklists. `is_virtual` has no field in extension_api.json, and does
# not need one -- ClassDB::_can_instantiate looks at creation_func, which a
# virtual class has none of, so is_instantiable already says no to those.
def offerable(name, classes):
    described = classes[name]
    return (described["api_type"] == "core" and
            described.get("is_instantiable") and
            name not in ("MissingNode", "MissingResource"))


# 03 の 5.2: a register is one byte and LHAT_MAX_REGISTERS is 250, so a unit
# holds fewer than that many bindings. The whole Node tree is 251, so the
# wrappers are written one file per line of godot-classes.txt -- a line names
# a branch and the file is that branch. Nothing composes between wrappers
# (each holds its own class's handle), so where a wrapper lives is free.
MAX_PER_UNIT = 240


def selected(classes):
    """[(group name, [class, ...]), ...], in the order the list names them.

    A group carries the whole ancestry of what it names, so each file stands
    on its own: reading lhat/Godot/Node3D.lh is enough to write a Node3D or
    anything under it. The trunk therefore appears in more than one file, and
    that is fine -- nothing composes between wrappers and each holds its own
    class's handle, so two files declaring a wrapper for godot.Node declare
    the same shape over the same host type.

    A line that reaches nothing new after the lines before it is dropped, so
    `under Node` written last is the Nodes no branch above it covered.
    """
    groups = []
    covered = set()

    with open(CLASSES, encoding="utf-8") as source:
        for line in source:
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            leaves = []
            if line.startswith("under "):
                named = line[len("under "):].strip()
                if named not in classes:
                    sys.exit("no engine class named " + named)
                leaves = [n for n in classes
                          if named in chain(n, classes) and
                          offerable(n, classes)]
            else:
                named = line
                if named not in classes:
                    sys.exit("no engine class named " + named)
                leaves = [named]

            # Only what no earlier line already put in a file of its own.
            fresh = [n for n in leaves if n not in covered]
            if not fresh:
                continue
            covered.update(fresh)

            # The ancestry of what is new here, so the file stands alone
            # without carrying leaves an earlier file already wrote.
            here = []
            for leaf in fresh:
                for step in chain(leaf, classes):
                    if step not in here:
                        here.append(step)
            if len(here) > MAX_PER_UNIT:
                sys.exit("%s takes %d classes with its ancestry; a unit holds "
                         "%d. Name narrower branches."
                         % (named, len(here), MAX_PER_UNIT))
            groups.append((named, here))
    return groups


def every(groups):
    """Every class the groups name, once each, in order."""
    out = []
    for _, names in groups:
        for name in names:
            if name not in out:
                out.append(name)
    return out


def gather(api, classes, wanted):
    rows = []
    left_out = 0
    for name in wanted:
        for method in classes[name].get("methods", []):
            # A virtual is what a script implements rather than calls, a
            # vararg has no ptrcall, and a static one has no receiver to be
            # written first. None of the three belongs in this table.
            if (method.get("is_virtual") or method.get("is_vararg") or
                    method.get("is_static")):
                continue
            arguments = method.get("arguments", [])
            answered = method.get("return_value", {}).get("type")
            kinds = [kind_of(a["type"], classes, wanted) for a in arguments]
            answer = (kind_of(answered, classes, wanted) if answered
                      else ("LHAT_GD_NIL", None))
            if answer is None or any(kind is None for kind in kinds):
                left_out += 1
                continue
            # 14.4: a first parameter written self^ is the receiver, and a
            # call passes what stands before the dot there without writing it.
            written = ["self^"] + [kind[1] for kind in kinds]
            if answer[1] is None:
                signature = "p^" + ", ".join(written) + ";"
            else:
                signature = ("f^" + ", ".join(written) + " -> " +
                             answer[1] + ";")
            boxed = sum(1 for kind in kinds if kind[0] in BOXED)
            if boxed > MAX_BOXED:
                sys.exit("%s.%s wants %d built arguments; raise "
                         "LHAT_GD_MAX_BOXED past %d"
                         % (name, method["name"], boxed, MAX_BOXED))
            rows.append({
                "name": method["name"],
                "signature": signature,
                "class": name,
                "hash": method["hash"],
                "answer": answer[0],
                "boxed": boxed,
                "kinds": [kind[0] for kind in kinds],
            })
    return rows, left_out


def split_signature(signature, room=68):
    """The signature as one string per line, none of them wider than `room`."""
    lines = []
    line = ""
    for word in signature.split(" "):
        if line and len(line) + 1 + len(word) > room:
            lines.append(line + " ")
            line = word
        else:
            line = (line + " " + word) if line else word
    lines.append(line)
    return lines


def write(api, classes, wanted, rows, left_out):
    # The same argument shape recurs -- every setter of a float, every getter
    # of nothing -- so one array is written per distinct shape and shared.
    shapes = {}
    for row in rows:
        shape = tuple(row["kinds"])
        if shape and shape not in shapes:
            shapes[shape] = "arg_shape_%d" % len(shapes)

    out = []
    say = out.append
    say("// L^ (lhat) -- GENERATED by scripts/gen-godot-api.py. Do not edit.")
    say("//")
    say("// One entry per engine method a script may call: the compatibility")
    say("// hash that finds its MethodBind, and the kinds its arguments and")
    say("// its answer cross as (lhat_godot_module.h). Regenerate after")
    say("// changing scripts/godot-classes.txt, or after moving to another")
    say("// godot-cpp:")
    say("//")
    say("//     python scripts/gen-godot-api.py")
    say("//")
    say("// From %s." % api["header"]["version_full_name"])
    say("// %d classes, %d methods. %d more were left out because an argument"
        % (len(wanted), len(rows), left_out))
    say("// or an answer of theirs is an Array, a Dictionary or a typed")
    say("// array, and nothing stands for those yet.")
    say("")
    say('#include "lhat_godot_api.gen.h"')
    say("")
    say("#include <godot_cpp/godot.hpp>")
    say("#include <godot_cpp/variant/string_name.hpp>")
    say("")
    say('#include "lhat_godot_module.h"')
    say("")
    say("namespace godot {")
    say("namespace host {")
    say("namespace {")
    say("")
    for shape, label in shapes.items():
        say("const uint8_t %s[] = {" % label)
        line = "   "
        for kind in shape:
            piece = " " + kind + ","
            if len(line) + len(piece) > 79:
                say(line)
                line = "   " + piece
            else:
                line += piece
        say(line)
        say("};")
    say("")
    say("BoundClass classes[] = {")
    for name in wanted:
        base = classes[name].get("inherits")
        say('    {"%s", %s, nullptr},'
            % (name, ('"%s"' % base) if base in wanted else "nullptr"))
    say("};")
    say("")
    say("BoundMethod bound[] = {")
    for row in rows:
        shape = shapes[tuple(row["kinds"])] if row["kinds"] else "nullptr"
        say('    {"%s",' % row["name"])
        # C++ joins adjacent literals, so a signature too wide for one line
        # is written as several. The text is the same either way.
        pieces = split_signature(row["signature"])
        for index, piece in enumerate(pieces):
            tail = "," if index == len(pieces) - 1 else ""
            say('     "%s"%s' % (piece, tail))
        say('     "%s", %uu,' % (row["class"], row["hash"]))
        tail = "%s, %d, %d, %s, nullptr, nullptr}," % (
            row["answer"], len(row["kinds"]), row["boxed"], shape)
        if len(tail) + 5 > 79:
            say("     %s," % row["answer"])
            say("     %d, %d, %s, nullptr, nullptr},"
                % (len(row["kinds"]), row["boxed"], shape))
        else:
            say("     %s" % tail)
    say("};")
    say("")
    say("}  // namespace")
    say("")
    say("bool register_godot_classes(LhatProgram *program, Godot *module)")
    say("{")
    say("    // The base is always earlier in the table than what is declared")
    say("    // under it, since a chain is walked from its root.")
    say("    for (BoundClass &owner : classes) {")
    say("        owner.tag =")
    say("            owner.base == nullptr")
    say('                ? lhat_register_hostdata_type(program, "godot",')
    say("                                              owner.name)")
    say('                : lhat_register_hostdata_subtype(')
    say('                      program, "godot", owner.name, "godot",')
    say("                      owner.base);")
    say("        if (owner.tag == nullptr) {")
    say("            return false;")
    say("        }")
    say("        module->tags.insert(StringName(owner.name), owner.tag);")
    say("    }")
    say("    return true;")
    say("}")
    say("")
    say("bool register_godot_api(LhatProgram *program, Godot *module)")
    say("{")
    say("    for (BoundMethod &method : bound) {")
    say("        // 8.7 makes the module the process's, so a bind is found")
    say("        // once however many programs are registered into.")
    say("        if (method.module == nullptr) {")
    say("            method.module = module;")
    say("            StringName owner(method.class_name);")
    say("            StringName named(method.name);")
    say("            method.bind =")
    say("                internal::"
        "gdextension_interface_classdb_get_method_bind(")
    say("                    owner._native_ptr(), named._native_ptr(),")
    say("                    (GDExtensionInt)method.hash);")
    say("        }")
    say('        if (!lhat_register_member(program, "godot",')
    say("                                  method.class_name, method.name,")
    say("                                  method.signature, bound_call,")
    say("                                  &method)) {")
    say("            return false;")
    say("        }")
    say("    }")
    say("    return true;")
    say("}")
    say("")
    say("}  // namespace host")
    say("}  // namespace godot")
    say("")

    with open(OUT, "w", encoding="utf-8", newline="\n") as target:
        target.write("\n".join(out))
    print("%s: %d classes, %d methods, %d shapes, %d left out"
          % (os.path.relpath(OUT, ROOT), len(wanted), len(rows), len(shapes),
             left_out))


# 05 の 8.8 leaves the engine's tree to L^: one host type stands for every
# object, so what tells a Sprite2D from a Node is a def^ per class, composed
# with `..`. These are the members of the root that are not engine methods --
# what holds the object, what makes one, and the three questions asked of any
# object at all (lhat_godot_module.cpp registers them on godot.Object).
ROOT_MEMBERS = """\tself^{ abstract^gdobj : godot.Object },

\toverride^new = f^obj:godot.Object {
\t\tself^{ gdobj = obj }
\t},

\tgdBaseClass = "Object",

\tisValid = f^self^ -> bool^ { self^.gdobj.isValid() },
\tclassName = f^self^ -> string^ { self^.gdobj.className() },
\temit = p^self^, name:string^, ... { self^.gdobj.emit(name, ...) },
"""

# What ROOT_MEMBERS already binds. is_class is not among them: the engine
# answers it, so the bound one is written instead -- a hand-written member
# would only have been the slower way to the same answer.
ROOT_NAMES = {"gdobj", "new", "gdBaseClass", "isValid", "className", "emit"}


# 01 の 6 章: a hat binds to what follows it, and a .lh is written with no
# space after one. So the code below writes none -- a regeneration leaves what
# the formatter would write alone. A comment is prose and keeps its spaces.
# 05 の 8.8改 puts the engine's tree on the host side, so what is left for L^
# is one wrapper per class -- something a script can compose onto, and
# something to hang @export fields off. Every member of the engine class shows
# through delegate^ (02 の 14.7改2), which makes no forwarding procedures: the
# name is looked for through the handle when neither the instance nor the
# definition has it.
#
# No composition between the wrappers. A composed def^ cannot narrow an
# inherited field (override^ marks a member, not the fields), so a Sprite2D
# wrapper composed onto a Node2D one would still hold a godot.Node2D and
# delegate to that. Each wrapper holding its own class's handle relates them
# anyway: 14.10's width subtyping, since the host type inherited the base's
# members, and member conformance is covariant over the field.
WRAPPER = """public^let^{name} = def^{{
\tself^{{ abstract^gdobj : godot.{name} }},

\toverride^new = f^obj:godot.{name} {{
\t\tself^{{ gdobj = obj }}
\t}},

\t# 02 の 18: what the engine registers this class as.
\tgdBaseClass = "{name}",

\tdelegate^self^.gdobj
}}"""


def write_lh(api, classes, groups, rows):
    for named, names in groups:
        write_one_lh(api, named, names)
    print("%s: %d units, %d wrappers"
          % (os.path.relpath(LH_DIR, ROOT), len(groups),
             len(every(groups))))


def write_one_lh(api, named, wanted):
    out = []
    say = out.append
    say("# L^ (lhat) -- GENERATED by scripts/gen-godot-api.py. "
        "Do not edit.")
    say("#")
    say("# The wrappers under %s. One per engine class, and the tree" % named)
    say("# itself is on the host side (05 の 8.8改): godot.Sprite2D is")
    say("# declared under godot.Node2D and stands wherever one is asked for,")
    say("# so nothing here composes -- which is also why it does not matter")
    say("# which of these files a wrapper lives in.")
    say("#")
    say("# delegate^ (02 の 14.7改2) is what shows the class's members")
    say("# through the wrapper. It makes no forwarding procedures at all --")
    say("# the name is looked for through the handle -- so this file costs a")
    say("# few lines per class however many methods that class has.")
    say("#")
    say("# The members are spelt as the engine spells them: set_rotation, not")
    say("# setRotation. What a script writes is what Godot's own")
    say("# documentation says.")
    say("#")
    say("# From %s." % api["header"]["version_full_name"])
    say("")
    say("module^lhat.Godot.%s" % named)
    say("")
    say("import^godot")
    for name in wanted:
        say("")
        say(WRAPPER.format(name=name))
    say("")

    if not os.path.isdir(LH_DIR):
        os.makedirs(LH_DIR)
    where = os.path.join(LH_DIR, named + ".lh")
    with open(where, "w", encoding="utf-8", newline="\n") as target:
        target.write("\n".join(out))


def callable_methods(name, classes):
    """(bound, reachable) for one class's own methods."""
    reachable = 0
    for method in classes[name].get("methods", []):
        if (method.get("is_virtual") or method.get("is_vararg") or
                method.get("is_static")):
            continue
        reachable += 1
    return reachable


def unbound_reason(method, classes, wanted):
    """Why a method is not in the table, or None when it is."""
    types = [a["type"] for a in method.get("arguments", [])]
    answered = method.get("return_value", {}).get("type")
    if answered:
        types.append(answered)
    for spelling in types:
        if kind_of(spelling, classes, wanted) is None:
            if spelling.startswith("typedarray::"):
                return "typed array"
            if spelling.startswith("typeddictionary::"):
                return "typed dictionary"
            return spelling
    return None


def write_docs(api, classes, groups, rows):
    """docs/godot-classes.md: what reaches L^ and what does not."""
    wanted = every(groups)
    lives_in = {}
    for named, names in groups:
        for name in names:
            lives_in.setdefault(name, []).append(named)
    bound = {}
    for row in rows:
        bound[row["class"]] = bound.get(row["class"], 0) + 1

    # The three groups a reader wants apart: what a script can be worn by,
    # what only turns up as a type, and the rest.
    def worn(name):
        walk = chain(name, classes)
        return "Node" in walk or "Resource" in walk

    offered = {n for n in classes if offerable(n, classes) and worn(n)}
    appears = set()
    for described in classes.values():
        for method in described.get("methods", []):
            for a in method.get("arguments", []):
                if a["type"] in classes:
                    appears.add(a["type"])
            answered = method.get("return_value", {}).get("type")
            if answered in classes:
                appears.add(answered)
        for signal in described.get("signals", []):
            for a in signal.get("arguments", []):
                if a["type"] in classes:
                    appears.add(a["type"])
        for described_property in described.get("properties", []):
            if described_property["type"] in classes:
                appears.add(described_property["type"])

    out = []
    say = out.append
    say("# Godot のクラスと L^ のバインド")
    say("")
    say("`scripts/gen-godot-api.py` が生成する。手で書き換えない。")
    say("")
    say("対象は `scripts/godot-classes.txt` が選んだもの。1行足して")
    say("`python scripts/gen-godot-api.py` で増える。")
    say("")
    say("- エンジンのクラス: **%d**" % len(classes))
    say("- L^ に登録済み: **%d**（%s）"
        % (len(wanted),
           "、".join("`%s.lh` %d" % (named, len(names))
                     for named, names in groups)))
    say("- バインドされたメソッド: **%d**" % len(rows))
    say("")
    say("メソッド数は「バインド済み / 呼べるもの」。呼べるものからは")
    say("virtual（スクリプトが実装する側）、vararg（ptrcall が無い）、")
    say("static（レシーバが無い）を除いてある。落ちた分の理由は")
    say("引数か答えに `Array` / `Dictionary` / 型付き配列があること。")
    say("")

    def section(title, note, names):
        say("## %s" % title)
        say("")
        say(note)
        say("")
        say("| クラス | 親 | L^ | メソッド | require^ |")
        say("|---|---|---|---|---|")
        for name in sorted(names):
            described = classes[name]
            reachable = callable_methods(name, classes)
            if name in wanted:
                mark = "○"
                counted = "%d / %d" % (bound.get(name, 0), reachable)
                where = "、".join("`lhat/Godot/%s.lh`" % one
                                  for one in lives_in[name])
            else:
                mark = "—"
                counted = "0 / %d" % reachable
                where = "—"
            say("| `%s` | `%s` | %s | %s | %s |"
                % (name, described.get("inherits") or "—", mark, counted,
                   where))
        say("")

    section("スクリプトが着られるクラス",
            "エディタが新規スクリプトの親として提示するもの"
            "（`CreateDialog::_should_hide_type` を抜けたもの）。"
            "Node 派生と Resource 派生。",
            offered)
    section("型としてだけ現れるクラス",
            "どのメソッドの引数・答え・シグナル・プロパティにも出てくるが、"
            "親としては提示されないもの。抽象基底とシングルトンが主。",
            appears - offered)
    section("そのほか",
            "上のどちらでもないもの。エディタ専用 API、"
            "直接 `new` して使う RefCounted 派生など。",
            set(classes) - offered - appears)

    directory = os.path.dirname(DOCS_OUT)
    if not os.path.isdir(directory):
        os.makedirs(directory)
    with open(DOCS_OUT, "w", encoding="utf-8", newline="\n") as target:
        target.write("\n".join(out))
    print("%s: %d classes listed" % (os.path.relpath(DOCS_OUT, ROOT),
                                     len(classes)))


def main():
    with open(API, encoding="utf-8") as source:
        api = json.load(source)
    classes = {c["name"]: c for c in api["classes"]}
    groups = selected(classes)
    wanted = every(groups)
    rows, left_out = gather(api, classes, wanted)
    write(api, classes, wanted, rows, left_out)
    write_lh(api, classes, groups, rows)
    write_docs(api, classes, groups, rows)


if __name__ == "__main__":
    main()
