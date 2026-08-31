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
MAX_BOXED = 5

# A string of the engine's is three types and one L^ type: what a script
# writes is text, and which flavour the engine wanted is the boundary's
# business rather than the writer's. Which one still has to be carried --
# a ptrcall reads the bytes as the type that was declared.
STRINGS = {
    "String": "LHAT_GD_STRING",
    "StringName": "LHAT_GD_STRINGNAME",
    "NodePath": "LHAT_GD_NODEPATH",
}


# The Variant::Type an element is, which is what a typed array is told apart
# by at run time and what set_typed is handed to make a fresh one. A class is
# OBJECT and named separately; everything else is one of these.
VARIANT_KIND = {
    "int": "INT", "float": "FLOAT", "bool": "BOOL",
    "String": "STRING", "StringName": "STRING_NAME", "NodePath": "NODE_PATH",
    "Array": "ARRAY", "Dictionary": "DICTIONARY", "Variant": "NIL",
}


# The L^ name for Array[X]. After the element the ENGINE declares, not after
# what it resolves to in L^ -- two declarations that resolve alike are still
# two things the engine can hand back, and the run-time tag is found by what
# the engine says the elements are.
def array_named(element):
    return "ArrayOf" + element[0].upper() + element[1:]


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
    # 05 の 8.8: Godot's Array and Dictionary are handles like the packed
    # arrays -- neither has a seam a view could go through, so converting
    # would be the only other road and it costs a Variant per element per
    # call (lhat_godot_containers.h).
    if spelling == "Dictionary" or spelling.startswith("typeddictionary::"):
        return "LHAT_GD_HOSTDATA + Variant::DICTIONARY", "godot.Dictionary"
    if spelling == "Array":
        return "LHAT_GD_HOSTDATA + Variant::ARRAY", "godot.Array"
    if spelling.startswith("typedarray::"):
        # 8.8改: one type per element, declared under godot.Array. The kind
        # stays ARRAY -- every one of them answers to the bare tag, which is
        # the promise a subtype registration makes -- so it is the SIGNATURE
        # that carries the element type, and the checker that holds it.
        return ("LHAT_GD_HOSTDATA + Variant::ARRAY",
                "godot." + array_named(spelling[len("typedarray::"):]))
    if spelling in classes:
        # 8.8改: the class itself where it was registered, and its nearest
        # registered ancestor otherwise -- which is also the tag a value of
        # it crosses with, so the two answers never disagree.
        for step in reversed(chain(spelling, classes)):
            if step in wanted:
                return "LHAT_GD_OBJECT", "godot." + step
        return "LHAT_GD_OBJECT", "godot.Object"
    return None  # a raw pointer, a typed dictionary's key or value


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


def selected(classes, api):
    """(classes to register, singletons, what `!` dropped).

    In the order the list names them. Nothing here is split into files any
    more: 05 の 8.8改 put the engine's tree on the host side, so what a script
    writes against is godot.Sprite2D itself and there is no wrapper unit for
    a register file to run out of.
    """
    wanted = []
    singletons = []
    # `!X` names a branch that is not wanted, and takes everything under X
    # with it. Whole branches rather than single classes, because a class
    # dropped out of the middle would leave what is under it declared under
    # something no longer registered -- and this way that cannot arise, since
    # anything with X in its chain goes when X does.
    #
    # Read here and applied at the end: `under` pulls ancestors up from many
    # leaves at once, so a rule that depended on where the line sat would not
    # be readable. A branch is in or it is not.
    excluded = []
    # 05 の 8.7: a singleton is a class the engine holds one instance of, and
    # a script never has that instance -- GDScript writes the class name and
    # calls through it. So these are not part of the class tree at all: they
    # become functions of a module of their own.
    every_singleton = [s["name"] for s in api.get("singletons", [])
                       if classes[s["name"]]["api_type"] == "core"]

    with open(CLASSES, encoding="utf-8") as source:
        for line in source:
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            if line.startswith("!"):
                named = line[1:].strip()
                if named not in classes:
                    sys.exit("no engine class named " + named)
                excluded.append(named)
                continue
            if line == "editor":
                # The editor half of `under Object`: every editor-API class
                # the engine can make, and every ancestor of one. Kept a word
                # of its own rather than folded into `under`, because
                # offerable() is what says "the engine can make this" and
                # the api_type test in it is what keeps the editor out of
                # every other line here.
                for name in classes:
                    if (classes[name]["api_type"] == "core" or
                            not classes[name].get("is_instantiable")):
                        continue
                    for step in chain(name, classes):
                        if (classes[step]["api_type"] != "core" and
                                step not in wanted):
                            wanted.append(step)
                continue
            if line == "singletons":
                for name in every_singleton:
                    if name not in singletons:
                        singletons.append(name)
                continue
            if line.startswith("singleton "):
                named = line[len("singleton "):].strip()
                if named not in every_singleton:
                    sys.exit("%s is not a core singleton" % named)
                if named not in singletons:
                    singletons.append(named)
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

            for leaf in leaves:
                for step in chain(leaf, classes):
                    if step not in wanted:
                        wanted.append(step)

    # 05 の 8.7 makes a singleton a module of its own, so it cannot also be a
    # type: godot.Input would name both, and the second registration is
    # refused. `under Object` reaches 26 of them -- Engine, OS, ClassDB and
    # the rest are ordinary instantiable classes as far as ClassDB is
    # concerned -- so this is where they come back out.
    #
    # What derives from one is declared under the nearest ancestor that is
    # still a type (write() walks for it): PhysicsServer2DExtension loses its
    # link to PhysicsServer2D and lands under Object, which is what
    # kind_of already answers for anything unregistered.
    wanted = [n for n in wanted if n not in singletons]

    dropped = set()
    for named in excluded:
        branch = {n for n in wanted if named in chain(n, classes)}
        branch |= {n for n in singletons if named in chain(n, classes)}
        # A `!` that drops nothing is a line left behind by an edit above it,
        # and the next reader would take it for a branch that is out when it
        # is a branch that was never in.
        if not branch:
            sys.exit("!%s drops nothing -- no line above it reaches %s"
                     % (named, named))
        dropped |= branch
    wanted = [n for n in wanted if n not in dropped]
    singletons = [n for n in singletons if n not in dropped]
    return wanted, singletons, dropped


def gather(api, classes, wanted, singletons=()):
    rows = []
    arrays = []
    left_out = 0
    for name in list(wanted) + list(singletons):
        alone = name in singletons
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
            # A method declared to answer a RefCounted answers a Ref<T>, and
            # PtrToArg<Ref<T>>::encode assigns into what it takes for one --
            # so the count comes back raised by one and that one is the
            # caller's. Which is godot-cpp's own rule: binding_generator.py
            # branches on is_refcounted(return_type) and reaches for
            # Ref::_gde_internal_constructor there and nowhere else.
            #
            # The DECLARED type settles it. A method declared to answer a
            # plain Object raises nothing, so reading the answer's own class
            # at run time would give a count back that was never taken.
            if (answer[0] == "LHAT_GD_OBJECT" and answered in classes and
                    classes[answered].get("is_refcounted")):
                answer = ("LHAT_GD_REFCOUNTED", answer[1])
            # 14.4: a first parameter written self^ is the receiver, and a
            # call passes what stands before the dot there without writing
            # it. A singleton's method has no receiver to write.
            written = ([] if alone else ["self^"]) + [k[1] for k in kinds]
            head = "p^" if answer[1] is None else "f^"
            signature = head + ", ".join(written)
            if answer[1] is not None:
                signature += (" -> " if written else " -> ") + answer[1]
            signature += ";"
            boxed = sum(1 for kind in kinds if kind[0] in BOXED)
            if boxed > MAX_BOXED:
                sys.exit("%s.%s wants %d built arguments; raise "
                         "LHAT_GD_MAX_BOXED past %d"
                         % (name, method["name"], boxed, MAX_BOXED))
            for spelling in ([a["type"] for a in arguments] +
                             ([answered] if answered else [])):
                if (spelling.startswith("typedarray::") and
                        spelling not in arrays):
                    arrays.append(spelling)
            # A row belongs to the editor when its class does, or when any
            # type it names does: an exported game has neither, so the two
            # have to leave together or a signature would name a type that
            # was never registered.
            editor = classes[name]["api_type"] != "core"
            for spelling in ([a["type"] for a in arguments] +
                             ([answered] if answered else [])):
                bare = spelling.replace("typedarray::", "")
                if bare in classes and classes[bare]["api_type"] != "core":
                    editor = True
            rows.append({
                "editor": editor,
                "name": method["name"],
                "signature": signature,
                "class": name,
                "module": ("godot." + name) if alone else None,
                "hash": method["hash"],
                "answer": answer[0],
                "boxed": boxed,
                "kinds": [kind[0] for kind in kinds],
            })
    return rows, arrays, left_out


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


def write(api, classes, wanted, rows, arrays, left_out):
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
    say("// %d classes, %d methods, %d typed arrays. %d methods were left"
        % (len(wanted), len(rows), len(arrays), left_out))
    say("// out because an argument or an answer of theirs is a raw pointer")
    say("// or a typed dictionary's key or value, neither of which anything")
    say("// here stands for.")
    say("")
    say('#include "lhat_godot_api.gen.h"')
    say("")
    say("#include <godot_cpp/core/class_db.hpp>")
    say("#include <godot_cpp/godot.hpp>")
    say("#include <godot_cpp/variant/string_name.hpp>")
    say("")
    say('#include "lhat_godot_containers.h"')
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
    # 05 の 8.7 registers before checking, and an exported game has no
    # editor API at all -- so what belongs to it is written into tables of
    # its own and skipped where ClassDB does not have it. Two arrays rather
    # than a flag per row: the skipping is then one branch instead of
    # fourteen thousand.
    held = set(wanted)
    def base_of(name):
        # The nearest ancestor still registered, rather than the immediate
        # parent: a singleton in between is a module and not a type, and
        # skipping it keeps the tree whole instead of breaking it into roots.
        base = classes[name].get("inherits")
        while base is not None and base not in held:
            base = classes[base].get("inherits")
        return ('"%s"' % base) if base is not None else "nullptr"

    for label, editor in (("classes", False), ("editor_classes", True)):
        say("BoundClass %s[] = {" % label)
        for name in wanted:
            if (classes[name]["api_type"] != "core") != editor:
                continue
            say('    {"%s", %s, nullptr},' % (name, base_of(name)))
        say("};")
        say("")

    for label, editor in (("bound", False), ("editor_bound", True)):
        say("BoundMethod %s[] = {" % label)
        for row in rows:
            if row["editor"] != editor:
                continue
            shape = (shapes[tuple(row["kinds"])] if row["kinds"]
                     else "nullptr")
            say('    {"%s",' % row["name"])
            # C++ joins adjacent literals, so a signature too wide for one
            # line is written as several. The text is the same either way.
            pieces = split_signature(row["signature"])
            for index, piece in enumerate(pieces):
                tail = "," if index == len(pieces) - 1 else ""
                say('     "%s"%s' % (piece, tail))
            say('     "%s", %s, %uu,'
                % (row["class"],
                   ('"%s"' % row["module"]) if row["module"] else "nullptr",
                   row["hash"]))
            tail = "%s, %d, %d, %s, nullptr, nullptr, nullptr}," % (
                row["answer"], len(row["kinds"]), row["boxed"], shape)
            if len(tail) + 5 > 79:
                say("     %s," % row["answer"])
                say("     %d, %d, %s, nullptr, nullptr, nullptr},"
                    % (len(row["kinds"]), row["boxed"], shape))
            else:
                say("     %s" % tail)
        say("};")
        say("")
    say("}  // namespace")
    say("")
    say("// 8.8改: one type per element a bound method asks for, each declared")
    say("// under godot.Array. The element type is the checker's; the tag is")
    say("// what a value coming back is told apart by, found from what the")
    say("// engine says its elements are (lhat_godot_containers.cpp).")
    # An array of an editor class leaves with the editor classes, for the
    # reason a row does: an exported game has neither, and declaring one
    # whose element type was never registered fails the whole registration.
    def editor_element(element):
        return (element in classes and
                classes[element]["api_type"] != "core")

    for label, editor in (("typed_arrays", False),
                          ("editor_typed_arrays", True)):
        say("BoundContainer %s[] = {" % label)
        for spelling in arrays:
            element = spelling[len("typedarray::"):]
            if editor_element(element) != editor:
                continue
            written = kind_of(element, classes, wanted)
            if written is None:
                sys.exit("nothing stands for an element of type " + element)
            if element in classes:
                kind, named = "OBJECT", '"%s"' % element
            else:
                if element not in VARIANT_KIND and element not in HOSTVALUE                         and element not in HOSTDATA:
                    sys.exit("no Variant kind for an element of type "
                             + element)
                kind = (VARIANT_KIND.get(element) or HOSTVALUE.get(element) or
                        HOSTDATA.get(element))
                named = "nullptr"
            say('    {"%s", "%s", "Array",'
                % (array_named(element), written[1]))
            say("     Variant::%s, %s, nullptr}," % (kind, named))
        say("};")
        say("")
    say("const size_t typed_array_count =")
    say("    sizeof(typed_arrays) / sizeof(typed_arrays[0]);")
    say("const size_t editor_typed_array_count =")
    say("    sizeof(editor_typed_arrays) / sizeof(editor_typed_arrays[0]);")
    say("")
    say("// Whether this build has the editor API at all. An editor binary")
    say("// does -- including the game it launches, which is the same")
    say("// binary -- and an exported template does not, so what was")
    say("// registered for the editor is skipped there rather than bound to")
    say("// a class that is not present. Asked once: ClassDB answers the")
    say("// same thing every time.")
    say("bool has_editor_api()")
    say("{")
    say("    static const bool it =")
    say('        ClassDB::class_exists("EditorScenePostImport");')
    say("    return it;")
    say("}")
    say("")
    say("bool declare(LhatProgram *program, Godot *module,")
    say("             BoundClass *owners, size_t how_many)")
    say("{")
    say("    // The base is always earlier in the table than what is declared")
    say("    // under it, since a chain is walked from its root.")
    say("    for (size_t at = 0; at < how_many; at++) {")
    say("        BoundClass &owner = owners[at];")
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
    say("bool register_godot_classes(LhatProgram *program, Godot *module)")
    say("{")
    say("    if (!declare(program, module, classes,")
    say("                 sizeof(classes) / sizeof(classes[0]))) {")
    say("        return false;")
    say("    }")
    say("    return !has_editor_api() ||")
    say("           declare(program, module, editor_classes,")
    say("                   sizeof(editor_classes) /")
    say("                       sizeof(editor_classes[0]));")
    say("}")
    say("")
    say("bool bind(LhatProgram *program, Godot *module, BoundMethod *methods,")
    say("          size_t how_many)")
    say("{")
    say("    for (size_t at = 0; at < how_many; at++) {")
    say("        BoundMethod &method = methods[at];")
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
    say("            if (method.module_path != nullptr) {")
    say("                // The one instance the engine holds. NULL where a")
    say("                // server is not compiled into this build, which")
    say("                // bound_call reads as an object that is not there.")
    say("                method.owner = (GodotObject *)internal::")
    say("                    gdextension_interface_global_get_singleton(")
    say("                        owner._native_ptr());")
    say("            }")
    say("        }")
    say('        if (method.module_path != nullptr) {')
    say("            if (!lhat_register_func(program, method.module_path,")
    say("                                    method.name, method.signature,")
    say("                                    bound_call, &method)) {")
    say("                return false;")
    say("            }")
    say("            continue;")
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
    say("bool register_godot_api(LhatProgram *program, Godot *module)")
    say("{")
    say("    if (!bind(program, module, bound,")
    say("              sizeof(bound) / sizeof(bound[0]))) {")
    say("        return false;")
    say("    }")
    say("    return !has_editor_api() ||")
    say("           bind(program, module, editor_bound,")
    say("                sizeof(editor_bound) / sizeof(editor_bound[0]));")
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


def write_docs(api, classes, registered, singletons, rows, dropped=()):
    """docs/godot-classes.md: what reaches L^ and what does not."""
    wanted = list(registered) + list(singletons)
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
    say("- L^ に登録済み: **%d**（うち singleton %d）"
        % (len(wanted), len(singletons)))
    say("- バインドされたメソッド: **%d**" % len(rows))
    say("")
    say("メソッド数は「バインド済み / 呼べるもの」。呼べるものからは")
    say("virtual（スクリプトが実装する側）、vararg（ptrcall が無い）、")
    say("static（レシーバが無い）を除いてある。")
    say("")
    say("`Array` は `godot.Array`、`Array[X]` は `godot.ArrayOfX`、")
    say("`Dictionary` は `godot.Dictionary`。どれも変換ではなくハンドルで、")
    say("要素は読んだときだけ渡る。`godot.ArrayOfX` は `godot.Array` の下に")
    say("宣言してあるので、型付きは型無しの場所に立てる（逆は立てない）。")
    say("")
    say("L^ 欄の `—` は「まだ無い」、`除外` は `godot-classes.txt` の")
    say("`!` 行で「要らないと決めた」枝。")
    say("")

    def section(title, note, names):
        say("## %s" % title)
        say("")
        say(note)
        say("")
        say("| クラス | 親 | L^ | メソッド | 書き方 |")
        say("| --- | --- | --- | --- | --- |")  # the spelling markdownlint keeps
        for name in sorted(names):
            described = classes[name]
            reachable = callable_methods(name, classes)
            if name in wanted:
                mark = "○"
                counted = "%d / %d" % (bound.get(name, 0), reachable)
                if name in singletons:
                    where = "`godot.%s.<method>()`" % name
                else:
                    where = "`godot.%s`" % name
            else:
                # 「まだ無い」と「要らないと決めた」は別のこと。
                mark = "除外" if name in dropped else "—"
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
    wanted, singletons, dropped = selected(classes, api)
    rows, arrays, left_out = gather(api, classes, wanted, singletons)
    write(api, classes, wanted, rows, arrays, left_out)
    write_docs(api, classes, wanted, singletons, rows, dropped)


if __name__ == "__main__":
    main()
