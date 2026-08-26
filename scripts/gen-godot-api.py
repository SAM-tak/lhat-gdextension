"""L^ (lhat) -- generate the bound engine methods (src/lhat_godot_api.gen.cpp).

05 の 8.7: the checker has to be told what a host registered, and the engine
has to be told which method a call means. Both are settled here, ahead of
time, out of extension_api.json -- the file godot-cpp generates its own
bindings from.

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
LH_OUT = os.path.join(ROOT, "demo", "lhat", "Godot.lh")

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

# A string of the engine's is three types and one L^ type: what a script
# writes is text, and which flavour the engine wanted is the boundary's
# business rather than the writer's.
STRINGS = {"String", "StringName", "NodePath"}


def kind_of(spelling, classes):
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
        return "LHAT_GD_STRING", "string^"
    if spelling == "Variant":
        return "LHAT_GD_VARIANT", "any^"
    if spelling in HOSTVALUE:
        return ("LHAT_GD_HOSTVALUE + Variant::" + HOSTVALUE[spelling],
                "godot." + spelling)
    if spelling in HOSTDATA:
        return ("LHAT_GD_HOSTDATA + Variant::" + HOSTDATA[spelling],
                "godot." + spelling)
    if spelling in classes:
        # 8.8: every engine object is the one host type, so a parameter that
        # wants a Node and one that wants a Sprite2D are written the same.
        # What refuses the wrong one is the def^ tree in L^ (lhat/Godot.lh).
        return "LHAT_GD_OBJECT", "godot.Object"
    return None  # Array, Dictionary, typedarray:: -- nothing stands for these


def camel(name):
    """The engine's spelling as L^ writes one: add_child -> addChild."""
    head, *rest = name.split("_")
    return head + "".join(part[:1].upper() + part[1:] for part in rest)


# 02 の 13.4: a default does not make an argument optional -- a call always
# writes as many as the declaration does. What it is for is the editing side,
# which puts it into the call it writes out. So one is carried over wherever
# it can be spelt in L^, and simply left off where it cannot: a Vector2(0, 0)
# is a construction rather than a literal, and nothing is lost by not
# offering it as the first thing to write.
def default_of(argument):
    written = argument.get("default_value")
    if written is None:
        return None
    if written == "false":
        return "false^"
    if written == "true":
        return "true^"
    if written in ('""', '&""', '^""'):
        return '""'
    try:
        int(written)
        return written
    except ValueError:
        pass
    try:
        float(written)
        return written
    except ValueError:
        return None


def chain(name, classes):
    walk = []
    while name:
        walk.append(name)
        name = classes[name].get("inherits")
    return walk[::-1]


def selected(classes):
    wanted = []
    with open(CLASSES, encoding="utf-8") as source:
        for line in source:
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            if line not in classes:
                sys.exit("no engine class named " + line)
            for step in chain(line, classes):
                if step not in wanted:
                    wanted.append(step)
    return wanted


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
            kinds = [kind_of(a["type"], classes) for a in arguments]
            answer = (kind_of(answered, classes) if answered
                      else ("LHAT_GD_NIL", None))
            if answer is None or any(kind is None for kind in kinds):
                left_out += 1
                continue
            written = ["godot.Object"] + [kind[1] for kind in kinds]
            if answer[1] is None:
                signature = "p^" + ", ".join(written) + ";"
            else:
                signature = ("f^" + ", ".join(written) + " -> " +
                             answer[1] + ";")
            rows.append({
                "module": "godot.api." + name,
                "name": method["name"],
                "signature": signature,
                "class": name,
                "hash": method["hash"],
                "answer": answer[0],
                "kinds": [kind[0] for kind in kinds],
                # What the L^ wrapper is written from.
                "written": camel(method["name"]),
                "answers": answer[1],
                "parameters": [
                    (camel(a["name"]), kind[1], default_of(a))
                    for a, kind in zip(arguments, kinds)
                ],
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


def write(api, wanted, rows, left_out):
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
    say("BoundMethod bound[] = {")
    for row in rows:
        shape = shapes[tuple(row["kinds"])] if row["kinds"] else "nullptr"
        say('    {"%s", "%s",' % (row["module"], row["name"]))
        # C++ joins adjacent literals, so a signature too wide for one line
        # is written as several. The text is the same either way.
        pieces = split_signature(row["signature"])
        for index, piece in enumerate(pieces):
            tail = "," if index == len(pieces) - 1 else ""
            say('     "%s"%s' % (piece, tail))
        say('     "%s", %uu,' % (row["class"], row["hash"]))
        tail = "%s, %d, %s, nullptr, nullptr}," % (row["answer"],
                                                   len(row["kinds"]), shape)
        if len(tail) + 5 > 79:
            say("     %s," % row["answer"])
            say("     %d, %s, nullptr, nullptr}," % (len(row["kinds"]), shape))
        else:
            say("     %s" % tail)
    say("};")
    say("")
    say("}  // namespace")
    say("")
    say("bool register_godot_api(LhatProgram *program, const Godot *module)")
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
    say("        if (!lhat_register_func(program, method.module_path,")
    say("                                method.name, method.signature,")
    say("                                bound_call, &method)) {")
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


def folded(tabs, head, pieces, room=79):
    """`head` then the pieces: one line where they fit, folded where not.

    A tab counts four, which is what the rest of the tree is written to, and
    every join is one space -- the caller puts its own commas on.
    """
    indent = "\t" * tabs
    following = "\t" * (tabs + 2)
    lines = []
    line = indent + head
    width = tabs * 4 + len(head)
    for piece in pieces:
        if width + 1 + len(piece) > room:
            lines.append(line)
            line = following + piece
            width = (tabs + 2) * 4 + len(piece)
        else:
            line += " " + piece
            width += 1 + len(piece)
    lines.append(line)
    return lines


def write_lh(api, classes, wanted, rows):
    by_class = {}
    for row in rows:
        by_class.setdefault(row["class"], []).append(row)

    out = []
    say = out.append
    say("# L^ (lhat) -- GENERATED by scripts/gen-godot-api.py. Do not edit.")
    say("#")
    say("# The engine's class tree, written as L^ definitions. 05 の 8.8 makes")
    say("# a host type nominal and 971 engine classes have no nominal shape L^")
    say("# could be given, so there is one host type and the hierarchy is")
    say("# here -- a def^ per class, composed with `..`, which 14.10's width")
    say("# subtyping relates the right way round.")
    say("#")
    say("# Every body reaches the engine through a bound method rather than")
    say("# by name (05 の 8.7), so a call costs a ptrcall and no lookup, and")
    say("# what comes back is already the type the signature says -- there is")
    say("# nothing here for as^ to narrow.")
    say("#")
    say("# From %s." % api["header"]["version_full_name"])
    say("")
    say("module^ lhat.Godot")
    say("")
    say("import^ godot")

    # A name a class writes over one of its own ancestors is a replacement
    # (14.15改), which is what override^ says. Along another branch it is
    # not: a Node3D's set_transform and a Node2D's are members of two
    # definitions that never meet, so what a class can see is its parent's
    # names and nobody else's.
    visible = {}
    for name in wanted:
        say("")
        inherits = classes[name].get("inherits")
        if inherits is None:
            say("public^let^ %s = def^{" % name)
            say(ROOT_MEMBERS.rstrip("\n"))
        else:
            say("public^let^ %s = %s..def^{" % (name, inherits))
            say('\toverride^gdBaseClass = "%s",' % name)
        here = set(visible.get(inherits, ROOT_NAMES))
        for row in by_class.get(name, []):
            written = row["written"]
            mark = "override^" if written in here else ""
            here.add(written)
            declared = []
            for spelt, typed, default in row["parameters"]:
                one = "%s:%s" % (spelt, typed)
                if default is not None:
                    one += " = " + default
                declared.append(one)
            tail = (" -> " + row["answers"] + " {") if row["answers"] else " {"
            head = "%s%s = %s%s" % (mark, written,
                                    "f^self^" if row["answers"] else "p^self^",
                                    "," if declared else tail)
            pieces = [one + "," for one in declared[:-1]]
            if declared:
                pieces.append(declared[-1] + tail)
            for line in folded(1, head, pieces):
                say(line)

            passed = [spelt for spelt, _, _ in row["parameters"]]
            opening = "%s.%s(self^.gdobj%s" % (row["module"], row["name"],
                                               "," if passed else ")")
            arguments = [one + "," for one in passed[:-1]]
            if passed:
                arguments.append(passed[-1] + ")")
            for line in folded(2, opening, arguments):
                say(line)
            say("\t},")
        say("}")
        visible[name] = here
    say("")

    with open(LH_OUT, "w", encoding="utf-8", newline="\n") as target:
        target.write("\n".join(out))
    print("%s: %d definitions" % (os.path.relpath(LH_OUT, ROOT), len(wanted)))


def main():
    with open(API, encoding="utf-8") as source:
        api = json.load(source)
    classes = {c["name"]: c for c in api["classes"]}
    wanted = selected(classes)
    rows, left_out = gather(api, classes, wanted)
    write(api, wanted, rows, left_out)
    write_lh(api, classes, wanted, rows)


if __name__ == "__main__":
    main()
