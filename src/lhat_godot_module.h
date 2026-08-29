// L^ (lhat) -- the `godot` module: a host type per engine class.
//
// 05 の 8.8改 lets a registered type say what it is under, so the engine's
// tree is the tree here: godot.Sprite2D is declared under godot.Node2D and
// stands wherever one is asked for. Before that there was one opaque
// godot.Object for all 971 classes and every boundary the checker could
// have held was lost with them.
//
// Which classes are here is scripts/godot-classes.txt, and what they carry
// is generated from extension_api.json into lhat_godot_api.gen.cpp. A class
// that was not registered is stood for by its nearest registered ancestor,
// so an engine object always crosses as something.
//
// What the type carries is an instance id and not a pointer. A node the
// program still holds may have been freed, and ObjectDB answers that
// question where a raw pointer would only be wrong: the id has a generation
// in it, so a recycled slot never answers to the old one. A RefCounted also
// gets a Ref held beside the id, since nothing else would keep it alive.

#ifndef LHAT_GODOT_MODULE_H
#define LHAT_GODOT_MODULE_H

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/list.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <gdextension_interface.h>

#include "lhat.h"

namespace godot {
namespace host {

// What one program's registration made. Belongs to the program and lives as
// long as it does.
struct Godot {
    const LhatHostDataTag *object_tag;
    // 05 の 8.8: a Callable and a Signal hold references rather than bytes,
    // so neither is one of 8.9's values (lhat_godot_handles.h).
    const LhatHostDataTag *callable_tag = nullptr;
    const LhatHostDataTag *signal_tag = nullptr;
    // 05 の 8.8: one tag per packed array, kept under the Variant kind it
    // stands for (lhat_godot_packed.h).
    const LhatHostDataTag *packed_tags[Variant::VARIANT_MAX] = {};

    // A registered name is borrowed rather than copied
    // (lhat_type_add_member keeps the pointer it is given), and the names
    // and signatures of the value types are built rather than written out.
    // So they are kept here -- and since this module is the process's, they
    // outlive every program that borrows them.
    //
    // The index is what makes a second registration cost nothing: the same
    // spelling hands back the same pointer, so registering into a fresh
    // program builds no string it built before.
    List<CharString> texts;
    HashMap<String, const char *> index;

    // 05 の 8.9: one tag per value type, kept under the Variant kind it
    // stands for -- so a host that has read a Variant can find the tag from
    // the type it found. NULL where nothing was registered.
    const LhatHostValueTag *value_tags[Variant::VARIANT_MAX] = {};

    // 05 の 8.8改: the tag an engine class crosses as, found by the name the
    // object answers to. A class nobody registered resolves to its nearest
    // registered ancestor and that answer is written here too, so the walk up
    // ClassDB happens once for the class rather than once per value.
    //
    // Mutable because remembering is not a change: what the table answers is
    // the same either way, and every reader but the registration is handed a
    // const module.
    mutable HashMap<StringName, const LhatHostDataTag *> tags;
};

// One engine class, declared under another. Generated; `tag` is filled by
// the first registration and is the process's from then on.
struct BoundClass {
    const char *name;
    const char *base;  // NULL for the root, which is Object
    const LhatHostDataTag *tag;
};

// 05 の 8.7: what an engine method costs to call, settled at registration
// instead of at the call. `godot.Object`'s `call` spells the method as a
// string, so every call pays a String, a StringName, an Array and a Variant
// per argument before ClassDB has even been asked. A bound method pays none
// of that: the MethodBind is found once and the arguments go through
// ptrcall, which takes the engine's own bytes.
//
// Generated, and it has to be: classdb_get_method_bind wants the
// compatibility hash, and the engine hands out no hash at run time (a
// MethodInfo carries none). scripts/gen-godot-api.py reads it out of
// extension_api.json, which is the same file godot-cpp generates itself
// from -- so the version the extension is built against is the version the
// hashes come from, and hash_compatibility is what carries them into a
// later engine.
//
// How one value crosses. A host value (8.9) is handed over as the bytes the
// machine already holds, which are the engine's own layout, so the case that
// matters converts nothing at all.
enum {
    LHAT_GD_NIL = 0,
    LHAT_GD_BOOL,
    LHAT_GD_INT,      // int, and every enum:: and bitfield::
    LHAT_GD_FLOAT,
    // A script writes text for all three, but a ptrcall is handed the type
    // the engine declared and they are three types: passing a String where a
    // StringName was declared hands the engine another object's bytes.
    LHAT_GD_STRING,
    LHAT_GD_STRINGNAME,
    LHAT_GD_NODEPATH,
    LHAT_GD_OBJECT,
    LHAT_GD_VARIANT,
    // An object the engine handed over a reference to, which is every method
    // declared to answer a RefCounted. Only ever an answer -- an argument of
    // that type crosses as LHAT_GD_OBJECT, since PtrToArg<Ref<T>>::convert
    // reads a bare pointer and takes a count of its own.
    LHAT_GD_REFCOUNTED,
    // The two that carry a Variant::Type in the low bits: one says which
    // value type's bytes these are (8.9), the other which handle it is
    // (a Callable, a Signal, one of the ten packed arrays).
    LHAT_GD_HOSTVALUE = 64,
    LHAT_GD_HOSTDATA = 128,
};

// The widest engine method takes 14 arguments (RenderingServer's fog), so a
// frame of this many covers every one of them and a call allocates nothing.
#define LHAT_GD_MAX_ARGS 16

// How many of those can want a String, a StringName, a NodePath or a Variant
// at once. These four are the arguments that have to be built rather than
// written into a machine word, and building a run of them per call was
// measured costing more than the ptrcall it prepares -- so they get a frame
// of their own, only as wide as any signature needs, and only where one
// does. The generator refuses a table that would overrun this.
#define LHAT_GD_MAX_BOXED 4

// One engine method, bound. Everything above `module` is generated and
// constant; the two below are filled by the first registration, which is
// what makes every registration after it free (the module is the process's).
struct BoundMethod {
    const char *name;       // the engine's own spelling, "add_child"
    const char *signature;  // 13 章's grammar, the receiver written self^
    const char *class_name;
    // Where a singleton's method is registered ("godot.Input"), and NULL for
    // a method of a class. A singleton is a class the engine holds one
    // instance of, and a script never has that instance in hand -- GDScript
    // writes `Input.is_action_pressed(…)` against the name alone. So these
    // are functions of a module rather than members of a type, the receiver
    // is not written, and `owner` below is where it comes from instead.
    const char *module_path;
    uint32_t hash;
    uint8_t answer;    // one of the kinds above
    uint8_t arg_count;
    // How many arguments have to be built (LHAT_GD_MAX_BOXED). Zero for five
    // methods in six, and those pay for no construction at all.
    uint8_t boxed;
    const uint8_t *arguments;  // arg_count kinds, the receiver not among them
    const Godot *module;
    GDExtensionMethodBindPtr bind;  // NULL where the engine refused the hash
    // The singleton this is called against, found once at registration.
    // NULL for everything else, which takes its receiver from the call.
    GodotObject *owner;
};

// What every entry of that table is registered with: reads `context` as the
// BoundMethod and ptrcalls what it names. 14.4 hands it the receiver first,
// which is what the signature's self^ says -- or does not, where the method
// is a singleton's and the receiver is the one the engine holds. Where the
// bind is NULL -- an engine that no longer answers to this hash -- it falls
// back to the call by name, so a version skew costs speed rather than the
// run.
void bound_call(LhatMachine *machine, void *context,
                const LhatValue *arguments, size_t count, LhatValue *answers,
                int *answer_count);

// Registers the module into `program`, before any checking (05 の 8.7).
// NULL when there was no memory or a name was taken.
//
// One module for the process. 05 の 8.7 makes a registration a declaration
// and interns what it declares, so a second program comes away with the very
// tags the first did -- there is nothing per-program in here to rebuild.
// What each program does keep is the checker's side of the registration,
// which is why the calls are made again for each; the strings they borrow
// are the ones built the first time.
const Godot *register_godot(LhatProgram *program);

// A registered name is borrowed rather than copied (lhat_type_add_member
// keeps the pointer it is given), and the value types' names and signatures
// are built rather than written out. This is where a built one goes: the
// module outlives every program, and the same spelling asked for twice hands
// back the one pointer, so registering into a second program builds nothing.
const char *kept(Godot *module, const String &text);

// Gives the module back. Only when no LhatProgram still borrows its strings,
// which means after every program is free -- the same window
// lhat_registry_dispose asks for, and the call belongs just before it.
void dispose_godot();

// A value standing for `object`, tagged with the nearest registered class it
// answers to (8.8改), or the one standing for nothing when it is NULL. False
// only when the machine ran out of memory.
//
// `adopt` is for a caller that already holds a reference to a RefCounted and
// is handing it over rather than lending it -- what a ptrcall answering a
// Ref<T> leaves behind. Without it the handle takes a second count and the
// first is never given back.
bool make_object(LhatMachine *machine, const Godot *module, Object *object,
                 LhatValue *out, bool adopt = false);

// The object such a value stands for, or NULL when the value is not one of
// ours, stands for nothing, or names something already freed. Any of the
// class tags answers, since lhat_hostdata_pointer walks to the base the
// caller asked for (8.8改).
Object *object_of(LhatValue value, const Godot *module);

// 02 の 18.7改: what a member marked @signal with an empty body is filled
// with. The name it emits and the module it converts arguments through --
// one of these per such member, made when a script is read and living as
// long as the machine holding the value that points at it.
struct SignalEmitter {
    const Godot *module = nullptr;
    StringName name;
};

// The value to put under that member's name: called like the member it
// replaces (`parameters` is what a caller writes, self^ not among them --
// 13.4), it emits `emitter`'s name off the receiver with whatever it was
// handed. The receiver is an instance of a class composed onto Godot.Object,
// so the engine object comes off its gdobj field.
//
// `emitter` is borrowed and has to outlive the value. False only when the
// machine ran out of memory.
bool make_signal_emitter(LhatMachine *machine, const SignalEmitter *emitter,
                         uint8_t parameters, bool variadic, LhatValue *out);

}  // namespace host
}  // namespace godot

#endif  // LHAT_GODOT_MODULE_H
