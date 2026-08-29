#include "lhat_godot_containers.h"

#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_float64_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_vector4_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>

#include "lhat_godot_module.h"
#include "lhat_godot_values.h"
#include "lhat_variant.h"

namespace godot {
namespace host {
namespace {

LhatValue element_missing(LhatMachine *machine, const BoundContainer *what);

// Every member is registered with its own container's row as context, so
// that `at` knows what its elements are as well as which module it is in.
const BoundContainer *container_of(void *context)
{
    return (const BoundContainer *)context;
}

const Godot *module_of(void *context)
{
    return container_of(context)->module;
}

// What the engine says a typed array's elements are, as one string, so a
// value coming back finds the type it was declared as. An object is named by
// its class and everything else by its Variant kind, which cannot collide --
// a class name is never digits.
String element_key(int64_t builtin, const StringName &class_name)
{
    return builtin == Variant::OBJECT ? String(class_name)
                                      : String::num_int64(builtin);
}

// One element the other way. 8.9's values are asked for first, because
// to_variant reads a box and a parameter written godot.Vector2i is handed
// the bytes themselves.
Variant element_taken(LhatValue value, const Godot *module)
{
    bool valued = false;
    Variant held = variant_of_value(value, module, &valued);
    return valued ? held : to_variant(value, module);
}

// What an element that is not there answers with. 04 の 11.3 says nothing,
// which an any^ can hold and a written type cannot -- a signature saying
// godot.Vector2i has no nil^ to give back. So this is the empty one of
// whatever the element is, which is what every packed array already answers
// past its end.
LhatValue element_missing(LhatMachine *machine, const BoundContainer *what)
{
    const Godot *module = what->module;
    const Variant::Type kind = (Variant::Type)what->builtin;
    // NIL is what the bare Array and every Dictionary carry: an any^, which
    // takes 11.3's answer as it stands.
    if (kind == Variant::NIL) {
        return lhat_nil();
    }
    bool zeroed = false;
    LhatValue made = value_zero(machine, module, kind, &zeroed);
    if (zeroed) {
        return made;
    }
    if (kind == Variant::OBJECT) {
        // 8.8's handle that stands for nothing, which is what an engine
        // object crosses as when there is none.
        LhatValue out = lhat_nil();
        return make_object(machine, module, nullptr, &out) ? out : lhat_nil();
    }
    // Everything left has a Variant standing for its own emptiness, and
    // element_answer knows how to cross one.
    switch (kind) {
        case Variant::BOOL:
            return element_answer(machine, what, false);
        case Variant::INT:
            return element_answer(machine, what, (int64_t)0);
        case Variant::FLOAT:
            return element_answer(machine, what, 0.0);
        case Variant::STRING:
            return element_answer(machine, what, String());
        case Variant::STRING_NAME:
            return element_answer(machine, what, StringName());
        case Variant::NODE_PATH:
            return element_answer(machine, what, NodePath());
        case Variant::ARRAY:
            return element_answer(machine, what, Array());
        case Variant::DICTIONARY:
            return element_answer(machine, what, Dictionary());
        case Variant::PACKED_BYTE_ARRAY:
            return element_answer(machine, what, PackedByteArray());
        case Variant::PACKED_INT32_ARRAY:
            return element_answer(machine, what, PackedInt32Array());
        case Variant::PACKED_INT64_ARRAY:
            return element_answer(machine, what, PackedInt64Array());
        case Variant::PACKED_FLOAT32_ARRAY:
            return element_answer(machine, what, PackedFloat32Array());
        case Variant::PACKED_FLOAT64_ARRAY:
            return element_answer(machine, what, PackedFloat64Array());
        case Variant::PACKED_STRING_ARRAY:
            return element_answer(machine, what, PackedStringArray());
        case Variant::PACKED_VECTOR2_ARRAY:
            return element_answer(machine, what, PackedVector2Array());
        case Variant::PACKED_VECTOR3_ARRAY:
            return element_answer(machine, what, PackedVector3Array());
        case Variant::PACKED_VECTOR4_ARRAY:
            return element_answer(machine, what, PackedVector4Array());
        case Variant::PACKED_COLOR_ARRAY:
            return element_answer(machine, what, PackedColorArray());
        default:
            return lhat_nil();
    }
}

// ---------------------------------------------------------------------------
// The members, written once and registered for every container

Array *held_array_of(LhatValue value, const Godot *module)
{
    return (Array *)lhat_hostdata_pointer(value,
                                          module->handle_tags[Variant::ARRAY]);
}

Dictionary *held_dictionary_of(LhatValue value, const Godot *module)
{
    return (Dictionary *)lhat_hostdata_pointer(
        value, module->handle_tags[Variant::DICTIONARY]);
}

void array_size(LhatMachine *machine, void *context,
                const LhatValue *arguments, size_t count, LhatValue *answers,
                int *answer_count)
{
    (void)machine;
    const Array *held =
        count > 0 ? held_array_of(arguments[0], module_of(context)) : nullptr;
    answers[0] = lhat_integer(held != nullptr ? held->size() : 0);
    *answer_count = 1;
}

// 04 の 11.3: an index that is not there answers nothing rather than
// stopping -- the same reading a table gives one.
void array_at(LhatMachine *machine, void *context, const LhatValue *arguments,
              size_t count, LhatValue *answers, int *answer_count)
{
    const Godot *module = module_of(context);
    const Array *held =
        count > 0 ? held_array_of(arguments[0], module) : nullptr;
    int64_t at = count > 1 && lhat_is_integer(arguments[1])
                     ? lhat_as_integer(arguments[1])
                     : 0;
    if (held == nullptr || at < 1 || at > held->size()) {
        answers[0] = element_missing(machine, container_of(context));
        *answer_count = 1;
        return;
    }
    // 02 の 14: a sequence is written from 1, so that is what is read here.
    answers[0] = element_answer(machine, container_of(context), (*held)[at - 1]);
    *answer_count = 1;
}

void array_set(LhatMachine *machine, void *context, const LhatValue *arguments,
               size_t count, LhatValue *answers, int *answer_count)
{
    (void)machine;
    const Godot *module = module_of(context);
    Array *held = count > 0 ? held_array_of(arguments[0], module) : nullptr;
    int64_t at = count > 1 && lhat_is_integer(arguments[1])
                     ? lhat_as_integer(arguments[1])
                     : 0;
    if (held == nullptr || count < 3 || at < 1 || at > held->size()) {
        return;
    }
    // A typed array refuses an element of the wrong type here, with the
    // engine's own error -- the checker has said what it can from the
    // signature, and what is left is what only the engine knows.
    held->set(at - 1, element_taken(arguments[2], module));
}

void array_append(LhatMachine *machine, void *context,
                  const LhatValue *arguments, size_t count, LhatValue *answers,
                  int *answer_count)
{
    (void)machine;
    const Godot *module = module_of(context);
    Array *held = count > 0 ? held_array_of(arguments[0], module) : nullptr;
    if (held == nullptr || count < 2) {
        return;
    }
    held->push_back(element_taken(arguments[1], module));
}

void array_clear(LhatMachine *machine, void *context,
                 const LhatValue *arguments, size_t count, LhatValue *answers,
                 int *answer_count)
{
    (void)machine;
    Array *held =
        count > 0 ? held_array_of(arguments[0], module_of(context)) : nullptr;
    if (held != nullptr) {
        held->clear();
    }
}

void array_dispose(LhatMachine *machine, void *context,
                   const LhatValue *arguments, size_t count,
                   LhatValue *answers, int *answer_count)
{
    (void)machine;
    Array *held =
        count > 0 ? held_array_of(arguments[0], module_of(context)) : nullptr;
    if (held != nullptr) {
        memdelete(held);
    }
}

// 02 の 16.3 with 05 の 8.8: `for^ x in^ a` runs this. The array rides as the
// coroutine's held value, which is what keeps the hostdata alive for the
// walk; the cursor re-reads it each step, so an append mid-walk is seen.
struct ArrayWalk {
    const BoundContainer *what;
    LhatValue over;
    int64_t at;  // 1-origin, the next element to hand over
};

bool array_step(LhatMachine *machine, void *context, const LhatValue *sent,
                size_t sent_count, LhatValue *answers, int *answer_count)
{
    (void)sent;  // the loops send nothing in
    (void)sent_count;
    ArrayWalk *walk = (ArrayWalk *)context;
    const Array *held = held_array_of(walk->over, walk->what->module);
    if (held == nullptr || walk->at > held->size()) {
        // 13.9's third slot: a walk that ends with nothing.
        return false;
    }
    answers[0] = element_answer(machine, walk->what, (*held)[walk->at - 1]);
    *answer_count = 1;
    walk->at++;
    return true;
}

// Under the dispose^ contract: once, and never reaching into the L^ API --
// the sweep may be the caller.
void array_walk_release(LhatMachine *machine, void *context,
                        const LhatValue *arguments, size_t count,
                        LhatValue *answers, int *answer_count)
{
    (void)machine;
    (void)arguments;
    (void)count;
    memdelete((ArrayWalk *)context);
}

void array_iterate(LhatMachine *machine, void *context,
                   const LhatValue *arguments, size_t count,
                   LhatValue *answers, int *answer_count)
{
    const Godot *module = module_of(context);
    if (count == 0 || held_array_of(arguments[0], module) == nullptr) {
        return;
    }
    ArrayWalk *walk = memnew(ArrayWalk);
    walk->what = container_of(context);
    walk->over = arguments[0];
    walk->at = 1;
    LhatValue out = lhat_nil();
    if (!lhat_machine_make_coroutine(machine, array_step, walk,
                                     array_walk_release, arguments[0], &out)) {
        memdelete(walk);
        return;
    }
    answers[0] = out;
    *answer_count = 1;
}

// ---------------------------------------------------------------------------
// The dictionary, which is keyed rather than counted

void dictionary_size(LhatMachine *machine, void *context,
                     const LhatValue *arguments, size_t count,
                     LhatValue *answers, int *answer_count)
{
    (void)machine;
    const Dictionary *held =
        count > 0 ? held_dictionary_of(arguments[0], module_of(context))
                  : nullptr;
    answers[0] = lhat_integer(held != nullptr ? held->size() : 0);
    *answer_count = 1;
}

void dictionary_at(LhatMachine *machine, void *context,
                   const LhatValue *arguments, size_t count,
                   LhatValue *answers, int *answer_count)
{
    const Godot *module = module_of(context);
    const Dictionary *held =
        count > 0 ? held_dictionary_of(arguments[0], module) : nullptr;
    if (held == nullptr || count < 2) {
        return;
    }
    // 04 の 11.3 again: a key that is not there answers the empty one.
    Variant key = element_taken(arguments[1], module);
    if (!held->has(key)) {
        answers[0] = element_missing(machine, container_of(context));
        *answer_count = 1;
        return;
    }
    answers[0] = element_answer(machine, container_of(context), (*held)[key]);
    *answer_count = 1;
}

void dictionary_set(LhatMachine *machine, void *context,
                    const LhatValue *arguments, size_t count,
                    LhatValue *answers, int *answer_count)
{
    (void)machine;
    const Godot *module = module_of(context);
    Dictionary *held =
        count > 0 ? held_dictionary_of(arguments[0], module) : nullptr;
    if (held == nullptr || count < 3) {
        return;
    }
    (*held)[element_taken(arguments[1], module)] =
        element_taken(arguments[2], module);
}

void dictionary_has(LhatMachine *machine, void *context,
                    const LhatValue *arguments, size_t count,
                    LhatValue *answers, int *answer_count)
{
    (void)machine;
    const Godot *module = module_of(context);
    const Dictionary *held =
        count > 0 ? held_dictionary_of(arguments[0], module) : nullptr;
    answers[0] = lhat_bool(held != nullptr && count > 1 &&
                           held->has(element_taken(arguments[1], module)));
    *answer_count = 1;
}

void dictionary_erase(LhatMachine *machine, void *context,
                      const LhatValue *arguments, size_t count,
                      LhatValue *answers, int *answer_count)
{
    (void)machine;
    const Godot *module = module_of(context);
    Dictionary *held =
        count > 0 ? held_dictionary_of(arguments[0], module) : nullptr;
    if (held != nullptr && count > 1) {
        held->erase(element_taken(arguments[1], module));
    }
}

void dictionary_keys(LhatMachine *machine, void *context,
                     const LhatValue *arguments, size_t count,
                     LhatValue *answers, int *answer_count)
{
    const Godot *module = module_of(context);
    const Dictionary *held =
        count > 0 ? held_dictionary_of(arguments[0], module) : nullptr;
    if (held == nullptr) {
        return;
    }
    LhatValue out = lhat_nil();
    if (!make_array(machine, module, held->keys(), &out)) {
        return;
    }
    answers[0] = out;
    *answer_count = 1;
}

void dictionary_clear(LhatMachine *machine, void *context,
                      const LhatValue *arguments, size_t count,
                      LhatValue *answers, int *answer_count)
{
    (void)machine;
    Dictionary *held =
        count > 0 ? held_dictionary_of(arguments[0], module_of(context))
                  : nullptr;
    if (held != nullptr) {
        held->clear();
    }
}

void dictionary_dispose(LhatMachine *machine, void *context,
                        const LhatValue *arguments, size_t count,
                        LhatValue *answers, int *answer_count)
{
    (void)machine;
    Dictionary *held =
        count > 0 ? held_dictionary_of(arguments[0], module_of(context))
                  : nullptr;
    if (held != nullptr) {
        memdelete(held);
    }
}

// The walk hands over keys, which is what a dictionary's own `for^` gives and
// what `at` is then written with.
struct DictionaryWalk {
    const BoundContainer *what;
    LhatValue over;
    Array keys;
    int64_t at;
};

bool dictionary_step(LhatMachine *machine, void *context,
                     const LhatValue *sent, size_t sent_count,
                     LhatValue *answers, int *answer_count)
{
    (void)sent;
    (void)sent_count;
    DictionaryWalk *walk = (DictionaryWalk *)context;
    if (walk->at > walk->keys.size()) {
        return false;
    }
    answers[0] =
        element_answer(machine, walk->what, walk->keys[walk->at - 1]);
    *answer_count = 1;
    walk->at++;
    return true;
}

void dictionary_walk_release(LhatMachine *machine, void *context,
                             const LhatValue *arguments, size_t count,
                             LhatValue *answers, int *answer_count)
{
    (void)machine;
    (void)arguments;
    (void)count;
    memdelete((DictionaryWalk *)context);
}

void dictionary_iterate(LhatMachine *machine, void *context,
                        const LhatValue *arguments, size_t count,
                        LhatValue *answers, int *answer_count)
{
    const Godot *module = module_of(context);
    const Dictionary *held =
        count > 0 ? held_dictionary_of(arguments[0], module) : nullptr;
    if (held == nullptr) {
        return;
    }
    // The keys are taken once, so a set mid-walk neither adds a step nor
    // drops one -- unlike the array's, whose cursor re-reads. A hash has no
    // stable order to re-read against.
    DictionaryWalk *walk = memnew(DictionaryWalk);
    walk->what = container_of(context);
    walk->over = arguments[0];
    walk->keys = held->keys();
    walk->at = 1;
    LhatValue out = lhat_nil();
    if (!lhat_machine_make_coroutine(machine, dictionary_step, walk,
                                     dictionary_walk_release, arguments[0],
                                     &out)) {
        memdelete(walk);
        return;
    }
    answers[0] = out;
    *answer_count = 1;
}

// A key read as one of 8.9's values. godot.Dictionary's own `at` cannot
// answer one: its element is an any^, and 8.9 keeps a value out of anything
// that outlives a frame. Naming the type in the member is what makes the
// answer a direct one, which 8.9 allows -- the same road
// godot.ArrayOfVector2i.at() already takes.
//
// A typed dictionary would not have answered this. What an engine hands back
// here is a raycast's result, whose keys are a Vector2, an Object, an int and
// a RID at once -- there is no one element type to declare, so the reader is
// the only one who can say which is wanted.
struct ValueAt {
    const BoundContainer *what;
    uint8_t kind;
};

void dictionary_at_value(LhatMachine *machine, void *context,
                         const LhatValue *arguments, size_t count,
                         LhatValue *answers, int *answer_count)
{
    const ValueAt *asked = (const ValueAt *)context;
    const Godot *module = asked->what->module;
    const Dictionary *held =
        count > 0 ? held_dictionary_of(arguments[0], module) : nullptr;
    Variant found;
    if (held != nullptr && count > 1) {
        Variant key = element_taken(arguments[1], module);
        if (held->has(key)) {
            found = (*held)[key];
        }
    }
    if (found.get_type() == (Variant::Type)asked->kind) {
        bool valued = false;
        LhatValue made = value_of_variant(machine, module, found, &valued);
        if (valued) {
            answers[0] = made;
            *answer_count = 1;
            return;
        }
    }
    // Not there, or there as something else. A signature naming the type has
    // no nil^ to give back, so it gives back the empty one -- has() is what
    // tells the two apart.
    bool zeroed = false;
    answers[0] =
        value_zero(machine, module, (Variant::Type)asked->kind, &zeroed);
    *answer_count = 1;
}

// One row per value type, filled at registration and the process's from then
// on -- a member is registered with its own row as context, which is how it
// knows which kind it was asked for.
ValueAt value_reads[Variant::VARIANT_MAX];

// ---------------------------------------------------------------------------
// Making one, and declaring the types

bool answer_array(LhatMachine *machine, const Godot *module, const Array &from,
                  const LhatHostDataTag *tag, LhatValue *out)
{
    Array *held = memnew(Array(from));
    if (!lhat_machine_make_hostdata(machine, tag, held, out)) {
        memdelete(held);
        return false;
    }
    return true;
}

// f^ -> godot.Array (and one per typed array): an empty one, typed as the
// name says so that handing it to the engine passes Array::assign outright.
void make_array_of(LhatMachine *machine, void *context,
                   const LhatValue *arguments, size_t count,
                   LhatValue *answers, int *answer_count)
{
    (void)arguments;
    (void)count;
    const BoundContainer *what = (const BoundContainer *)context;
    const Godot *module = what->module;
    const LhatHostDataTag *tag = module->handle_tags[Variant::ARRAY];
    if (what->base != nullptr) {
        const LhatHostDataTag *const *found = module->array_tags.getptr(
            element_key(what->builtin,
                        StringName(what->class_name ? what->class_name : "")));
        if (found != nullptr) {
            tag = *found;
        }
    }
    Array made;
    if (what->base != nullptr) {
        made.set_typed((uint32_t)what->builtin,
                       StringName(what->class_name ? what->class_name : ""),
                       Variant());
    }
    LhatValue out = lhat_nil();
    if (!answer_array(machine, module, made, tag, &out)) {
        return;
    }
    answers[0] = out;
    *answer_count = 1;
}

void make_dictionary_of(LhatMachine *machine, void *context,
                        const LhatValue *arguments, size_t count,
                        LhatValue *answers, int *answer_count)
{
    (void)arguments;
    (void)count;
    const Godot *module = container_of(context)->module;
    Dictionary *held = memnew(Dictionary);
    LhatValue out = lhat_nil();
    if (!lhat_machine_make_hostdata(
            machine, module->handle_tags[Variant::DICTIONARY], held, &out)) {
        memdelete(held);
        return;
    }
    answers[0] = out;
    *answer_count = 1;
}

struct Member {
    const char *name;
    const char *signature;
    LhatHostFn call;
};

bool declare_members(LhatProgram *program, BoundContainer *what,
                     const Member *members, size_t how_many)
{
    for (size_t i = 0; i < how_many; i++) {
        if (!lhat_register_member(program, "godot", what->name,
                                  members[i].name, members[i].signature,
                                  members[i].call, what)) {
            return false;
        }
    }
    return true;
}

// One name in L^ for one in the engine: get_children becomes getChildren, so
// a maker for godot.ArrayOfNode is arrayOfNode.
const char *maker_named(Godot *module, const char *name)
{
    String spelt = String(name);
    return kept(module, spelt.substr(0, 1).to_lower() + spelt.substr(1));
}

bool declare_array(LhatProgram *program, Godot *module,
                   BoundContainer *what)
{
    const char *element = what->element;
    what->module = module;
    const LhatHostDataTag *tag =
        what->base == nullptr
            ? lhat_register_hostdata_type(program, "godot", what->name)
            : lhat_register_hostdata_subtype(program, "godot", what->name,
                                             "godot", what->base);
    if (tag == nullptr) {
        return false;
    }
    if (what->base == nullptr) {
        module->handle_tags[Variant::ARRAY] = tag;
    } else {
        module->array_tags.insert(
            element_key(what->builtin,
                        StringName(what->class_name ? what->class_name : "")),
            tag);
    }

    const Member members[] = {
        {"size", kept(module, "f^self^ -> number^;"), array_size},
        {"at", kept(module, String("f^self^, number^ -> ") + element + ";"),
         array_at},
        {"set", kept(module, String("p^self^, number^, ") + element + ";"),
         array_set},
        {"append", kept(module, String("p^self^, ") + element + ";"),
         array_append},
        {"clear", kept(module, "p^self^;"), array_clear},
        // Bare `iterate`: on a host type the two spellings name one member
        // (14.17改), and every name here is the library's.
        {"iterate",
         kept(module, String("f^self^ -> c^{p^ -> ") + element + "};"),
         array_iterate},
    };
    if (!declare_members(program, what, members,
                         sizeof(members) / sizeof(members[0]))) {
        return false;
    }
    // 8.8改 reads the release through the base, so the bare one carries it
    // for every typed array declared under it.
    if (what->base == nullptr &&
        !lhat_register_member(program, "godot", what->name, "dispose",
                              kept(module, "p^self^;"), array_dispose,
                              what)) {
        return false;
    }
    return lhat_register_func(
        program, "godot", maker_named(module, what->name),
        kept(module, String("f^ -> godot.") + what->name + ";"), make_array_of,
        (void *)what);
}

bool declare_dictionary(LhatProgram *program, Godot *module,
                        BoundContainer *what)
{
    what->module = module;
    const LhatHostDataTag *tag =
        lhat_register_hostdata_type(program, "godot", what->name);
    if (tag == nullptr) {
        return false;
    }
    module->handle_tags[Variant::DICTIONARY] = tag;

    const Member members[] = {
        {"size", kept(module, "f^self^ -> number^;"), dictionary_size},
        {"at", kept(module, "f^self^, any^ -> any^;"), dictionary_at},
        {"set", kept(module, "p^self^, any^, any^;"), dictionary_set},
        {"has", kept(module, "f^self^, any^ -> bool^;"), dictionary_has},
        {"erase", kept(module, "p^self^, any^;"), dictionary_erase},
        {"keys", kept(module, "f^self^ -> godot.Array;"), dictionary_keys},
        {"clear", kept(module, "p^self^;"), dictionary_clear},
        {"dispose", kept(module, "p^self^;"), dictionary_dispose},
        {"iterate", kept(module, "f^self^ -> c^{p^ -> any^};"),
         dictionary_iterate},
    };
    if (!declare_members(program, what, members,
                         sizeof(members) / sizeof(members[0]))) {
        return false;
    }
    // atVector2, atVector3, ... -- one per value type the module registered,
    // named after the type so that they sort together next to `at`.
    for (int kind = 0; kind < Variant::VARIANT_MAX; kind++) {
        if (module->value_tags[kind] == nullptr) {
            continue;
        }
        // The engine's own spelling for the kind, which is what the value
        // type was registered under (lhat_godot_values.cpp's Named).
        String named = Variant::get_type_name((Variant::Type)kind);
        value_reads[kind].what = what;
        value_reads[kind].kind = (uint8_t)kind;
        if (!lhat_register_member(
                program, "godot", what->name, kept(module, "at" + named),
                kept(module, "f^self^, any^ -> godot." + named + ";"),
                dictionary_at_value, &value_reads[kind])) {
            return false;
        }
    }
    return lhat_register_func(program, "godot", "dictionary",
                              kept(module, "f^ -> godot.Dictionary;"),
                              make_dictionary_of, what);
}

// The bare two. Neither says what its elements are, so neither can carry one
// of 8.9's values -- see element_answer.
BoundContainer bare_array = {"Array", "any^", nullptr, Variant::NIL, nullptr,
                             nullptr};
BoundContainer bare_dictionary = {"Dictionary", "any^", nullptr, Variant::NIL,
                                  nullptr, nullptr};

}  // namespace

bool register_containers(LhatProgram *program, Godot *module)
{
    if (program == nullptr || module == nullptr) {
        return false;
    }
    if (!declare_array(program, module, &bare_array) ||
        !declare_dictionary(program, module, &bare_dictionary)) {
        return false;
    }
    for (size_t i = 0; i < typed_array_count; i++) {
        if (!declare_array(program, module, &typed_arrays[i])) {
            return false;
        }
    }
    return true;
}

bool make_array(LhatMachine *machine, const Godot *module, const Array &from,
                LhatValue *out)
{
    if (module == nullptr) {
        return false;
    }
    // What the engine says the elements are is what tells godot.ArrayOfNode
    // from godot.Array. An element type nothing was generated for reads as
    // the bare one, which every member still works on.
    const LhatHostDataTag *tag = module->handle_tags[Variant::ARRAY];
    if (from.is_typed()) {
        const LhatHostDataTag *const *found = module->array_tags.getptr(
            element_key(from.get_typed_builtin(), from.get_typed_class_name()));
        if (found != nullptr) {
            tag = *found;
        }
    }
    return answer_array(machine, module, from, tag, out);
}

bool make_dictionary(LhatMachine *machine, const Godot *module,
                     const Dictionary &from, LhatValue *out)
{
    if (module == nullptr) {
        return false;
    }
    Dictionary *held = memnew(Dictionary(from));
    if (!lhat_machine_make_hostdata(
            machine, module->handle_tags[Variant::DICTIONARY], held, out)) {
        memdelete(held);
        return false;
    }
    return true;
}

Array *held_array(LhatValue value, const Godot *module)
{
    return module != nullptr ? held_array_of(value, module) : nullptr;
}

Dictionary *held_dictionary(LhatValue value, const Godot *module)
{
    return module != nullptr ? held_dictionary_of(value, module) : nullptr;
}

LhatValue element_answer(LhatMachine *machine, const BoundContainer *what,
                         const Variant &held)
{
    const Godot *module = what->module;
    LhatValue out = lhat_nil();
    switch (held.get_type()) {
        // A container inside a container stays a container: from_variant
        // would make a table of it, which is the right answer for an any^
        // asked of the engine and the wrong one for what a signature calls
        // godot.Array.
        case Variant::ARRAY:
            return make_array(machine, module, (Array)held, &out) ? out
                                                                  : lhat_nil();
        case Variant::DICTIONARY:
            return make_dictionary(machine, module, (Dictionary)held, &out)
                       ? out
                       : lhat_nil();
        default:
            break;
    }
    // 8.9's values, and only where the signature named the type. A member
    // written `-> any^` may not answer one: 8.9 keeps a value out of
    // anything that outlives a frame, and the machine refuses the write
    // rather than letting it through. So the bare Array and every
    // Dictionary lose a Vector2 here -- the same way from_variant has always
    // lost one, and for the same reason.
    if (what->builtin < Variant::VARIANT_MAX &&
        module->value_tags[what->builtin] != nullptr) {
        bool valued = false;
        LhatValue made = value_of_variant(machine, module, held, &valued);
        if (valued) {
            return made;
        }
    }
    return from_variant(machine, held, &out, module) ? out : lhat_nil();
}

}  // namespace host
}  // namespace godot
