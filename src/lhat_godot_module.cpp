#include "lhat_godot_module.h"

#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "lhat_godot_api.gen.h"
#include "lhat_godot_handles.h"
#include "lhat_godot_packed.h"
#include "lhat_godot_values.h"
#include "lhat_variant.h"

namespace godot {
namespace host {

namespace {

// The box a godot.Object points at. An id rather than a pointer, for the
// reason at the head of the header; the Ref is empty for anything the engine
// owns (a Node lives in the tree) and holds for a RefCounted, which nothing
// else would keep alive while L^ has it.
struct Handle {
    uint64_t id = 0;
    Ref<RefCounted> hold;
};

// Allocated once per register_godot and never freed -- program.h has no hook
// to run when its LhatProgram is disposed, which is the same trade
// stdlib/io.c takes for the same reason.
const Godot *module_of(void *context)
{
    return (const Godot *)context;
}

Object *resolve(const Handle *handle)
{
    if (handle == nullptr || handle->id == 0) {
        return nullptr;
    }
    return UtilityFunctions::instance_from_id((int64_t)handle->id);
}

// The engine's object and nothing else, which is all a ptrcall is handed.
//
// resolve above goes the way godot-cpp goes: a utility function, and then
// get_object_instance_binding to hand back something with methods on it. A
// bound method calls none of them -- it has a MethodBind and wants the
// pointer to call it against -- so this asks ObjectDB and stops there.
//
// Worth a fifth of what isValid cost and about a twentieth of a bound call;
// ObjectDB itself was never the expensive part, and neither, it turns out, is
// a binding already made. What this buys is not paying for either.
//
// The id still answers the question a raw pointer could not: it carries a
// generation, so a freed object gives NULL here rather than a recycled slot.
GodotObject *owner_of(LhatValue value, const Godot *module)
{
    if (module == nullptr) {
        return nullptr;
    }
    const Handle *handle =
        (const Handle *)lhat_hostdata_pointer(value, module->object_tag);
    if (handle == nullptr || handle->id == 0) {
        return nullptr;
    }
    return (GodotObject *)internal::gdextension_interface_object_get_instance_from_id(
        (GDObjectInstanceID)handle->id);
}

Object *receiver(const LhatValue *arguments, size_t count, void *context)
{
    if (count == 0) {
        return nullptr;
    }
    const Godot *module = module_of(context);
    return object_of(arguments[0], module);
}

bool text_of(LhatValue value, String *out)
{
    if (!lhat_is_object_kind(value, LHAT_OBJECT_STRING)) {
        return false;
    }
    const LhatString *text = (const LhatString *)lhat_as_object(value);
    *out = String::utf8(text->text, (int)text->length);
    return true;
}

// What a host answers with when the answer is text. The three string types
// the engine declares are one string^ to a script, so all three come through
// here once they are back on this side.
LhatValue text_answer(LhatMachine *machine, const String &text)
{
    CharString bytes = text.utf8();
    LhatValue made = lhat_nil();
    lhat_machine_make_string(machine, bytes.get_data(), (size_t)bytes.length(),
                             &made);
    return made;
}

// 04 の 11.3 spells "not there" nil^, and a freed object is the same kind of
// absence. A script that cares asks isValid^ first, the way GDScript asks
// is_instance_valid -- making every reach a try^ would cost more than it
// buys here.
LhatValue gone(const char *what, LhatValue self, const Godot *module)
{
    (void)self;
    (void)module;
    UtilityFunctions::push_error(
        String("L^: ") + String(what) +
        String(" reached an object that is not there any more"));
    return lhat_nil();
}

// What a bound method falls back to: the call by name, which is what every
// engine method cost before there was a bind. Reached on a hash the engine
// refused and on an answer no ptrcall shape covers -- a version skew costs
// speed rather than the run.
LhatValue by_name(LhatMachine *machine, const BoundMethod *method,
                  Object *object, const LhatValue *arguments, size_t first,
                  size_t count)
{
    const Godot *module = method->module;
    Array passed;
    for (size_t i = first; i < count; i++) {
        passed.push_back(to_variant(arguments[i], module));
    }
    LhatValue made = lhat_nil();
    from_variant(machine, object->callv(method->name, passed), &made, module);
    return made;
}

// ---------------------------------------------------------------------------

void godot_is_valid(LhatMachine *machine, void *context,
                         const LhatValue *arguments, size_t count,
                         LhatValue *answers, int *answer_count)
{
    (void)machine;
    // Whether ObjectDB still answers to the id, which owner_of asks straight
    // -- nothing here calls a method on the object, so nothing here needs
    // godot-cpp's instance binding.
    answers[0] = lhat_bool(count > 0 &&
                     owner_of(arguments[0], module_of(context)) != nullptr);
    *answer_count = 1;
}

void godot_class_name(LhatMachine *machine, void *context,
                           const LhatValue *arguments, size_t count,
                           LhatValue *answers, int *answer_count)
{
    Object *object = receiver(arguments, count, context);
    if (object == nullptr) {
        answers[0] = gone("className", arguments[0], module_of(context));
        *answer_count = 1;
        return;
    }
    CharString text = object->get_class().utf8();
    LhatValue made = lhat_nil();
    lhat_machine_make_string(machine, text.get_data(), (size_t)text.length(),
                             &made);
    answers[0] = made;
    *answer_count = 1;
}

void godot_is_class(LhatMachine *machine, void *context,
                         const LhatValue *arguments, size_t count,
                         LhatValue *answers, int *answer_count)
{
    (void)machine;
    Object *object = receiver(arguments, count, context);
    String name;
    if (object == nullptr || count < 2 || !text_of(arguments[1], &name)) {
        answers[0] = lhat_bool(false);
        *answer_count = 1;
        return;
    }
    answers[0] = lhat_bool(object->is_class(name));
    *answer_count = 1;
}

// 13.7: the signature ends in '...', so the tail arrives as the arguments
// after the name -- uncollected, which is what a C function wants.
void godot_call(LhatMachine *machine, void *context,
                     const LhatValue *arguments, size_t count,
                     LhatValue *answers, int *answer_count)
{
    const Godot *module = module_of(context);
    Object *object = receiver(arguments, count, context);
    String name;
    if (object == nullptr) {
        answers[0] = gone("call", arguments[0], module);
        *answer_count = 1;
        return;
    }
    if (count < 2 || !text_of(arguments[1], &name)) {
        return;
    }

    Array passed;
    for (size_t i = 2; i < count; i++) {
        passed.push_back(to_variant(arguments[i], module));
    }
    LhatValue made = lhat_nil();
    from_variant(machine, object->callv(name, passed), &made, module);
    answers[0] = made;
    *answer_count = 1;
}

// The same shape as call, and separate because emitting is not calling: the
// name reaches every connection rather than one method. What a body marked
// @signal is written around.
void godot_emit(LhatMachine *machine, void *context,
                     const LhatValue *arguments, size_t count,
                     LhatValue *answers, int *answer_count)
{
    (void)machine;
    const Godot *module = module_of(context);
    Object *object = receiver(arguments, count, context);
    String name;
    if (object == nullptr) {
        answers[0] = gone("emit", arguments[0], module);
        *answer_count = 1;
        return;
    }
    if (count < 2 || !text_of(arguments[1], &name)) {
        return;
    }

    Array passed;
    for (size_t i = 2; i < count; i++) {
        passed.push_back(to_variant(arguments[i], module));
    }
    // emit_signal is variadic where callv is not, so the arguments go through
    // as one call rather than being spread here.
    passed.push_front(name);
    object->callv("emit_signal", passed);
}

// 02 の 18.7改: the engine object a receiver stands for. A godot.Object
// answers for itself; anything else is an instance of a class composed onto
// Godot.Object, and 14.15's declared field is where that class keeps the
// object -- the same `self^.gdobj` a hand-written body reaches through.
Object *engine_object_of(LhatMachine *machine, LhatValue self,
                         const Godot *module)
{
    Object *direct = object_of(self, module);
    if (direct != nullptr || !lhat_is_object_kind(self, LHAT_OBJECT_TABLE)) {
        return direct;
    }
    // Made per call, the way instance_get and instance_set make theirs: a
    // key kept between calls would be a value with no root, and the
    // collector reaches nothing a host holds.
    LhatValue key = lhat_nil();
    if (!lhat_machine_make_string(machine, "gdobj", 5, &key)) {
        return nullptr;
    }
    const LhatTable *table = (const LhatTable *)lhat_as_object(self);
    return object_of(lhat_table_get(table, key), module);
}

// What an empty-bodied @signal member is filled with. The same road
// godot_emit takes, with the name coming from the registration rather than
// from an argument -- which is the whole of what writing the body did.
void signal_emit(LhatMachine *machine, void *context,
                 const LhatValue *arguments, size_t count,
                 LhatValue *answers, int *answer_count)
{
    const SignalEmitter *emitter = (const SignalEmitter *)context;
    if (emitter == nullptr || count == 0) {
        return;
    }
    Object *object = engine_object_of(machine, arguments[0], emitter->module);
    if (object == nullptr) {
        answers[0] = gone("emit", arguments[0], emitter->module);
        *answer_count = 1;
        return;
    }

    // 13.4 leaves self^ out of what a caller writes, and arguments[0] is it,
    // so the signal's arguments are the rest.
    Array passed;
    passed.push_back(emitter->name);
    for (size_t i = 1; i < count; i++) {
        passed.push_back(to_variant(arguments[i], emitter->module));
    }
    object->callv("emit_signal", passed);
}

// 05 の 8.8: registering this is what makes the box the host's to hand over
// and L^'s to give back.
void godot_dispose(LhatMachine *machine, void *context,
                   const LhatValue *arguments, size_t count,
                   LhatValue *answers, int *answer_count)
{
    (void)machine;
    if (count == 0) {
        return;
    }
    Handle *handle =
        (Handle *)lhat_hostdata_pointer(arguments[0],
                                        module_of(context)->object_tag);
    if (handle != nullptr) {
        memdelete(handle);
    }
}

// True in the editor and false in a game, including one launched from it.
// What a @tool class asks when it wants to do one thing while a scene is
// being edited and another while it is played.
void godot_is_editor_hint(LhatMachine *machine, void *context,
                          const LhatValue *arguments, size_t count,
                          LhatValue *answers, int *answer_count)
{
    (void)machine;
    (void)context;
    (void)arguments;
    (void)count;
    answers[0] = lhat_bool(Engine::get_singleton()->is_editor_hint());
    *answer_count = 1;
}


// One module for the process, made on the first registration and given back
// by dispose_godot. Nothing in it is a program's: 05 の 8.7 interns what a
// registration declares, so the tags are the same whichever program asked,
// and the strings below are borrowed by every program that ever registers.
Godot *shared = nullptr;

}  // namespace

const char *kept(Godot *module, const String &text)
{
    if (const char *const *found = module->index.getptr(text)) {
        return *found;
    }
    module->texts.push_back(text.utf8());
    const char *held = module->texts.back()->get().get_data();
    module->index.insert(text, held);
    return held;
}

void dispose_godot()
{
    if (shared != nullptr) {
        memdelete(shared);
        shared = nullptr;
    }
}

// 05 の 8.7: one engine method, reached through the bind found when it was
// registered. What godot_call pays by name -- a String, a StringName, an
// Array, and a Variant for every argument, before ClassDB has even been
// asked -- is all paid ahead of the run here, and what crosses is the bytes
// the machine already holds (8.9).
//
// The receiver is written first, as the signature says. It is not self^:
// these are functions of a module rather than members of the one host type,
// which 971 classes of members would all have landed on.
// The room one call needs that is not a machine word. A String, a StringName,
// a NodePath and a Variant each have to be built, and building runs of them
// per call was measured costing more than the ptrcall they prepare -- so this
// stands apart from bound_call's frame and is put down only where a signature
// wants one. Five methods in six want none.
struct Boxed {
    String texts[LHAT_GD_MAX_BOXED];
    StringName names[LHAT_GD_MAX_BOXED];
    NodePath paths[LHAT_GD_MAX_BOXED];
    Variant variants[LHAT_GD_MAX_BOXED];
};

// What a ptrcall is handed for an argument that fits in a machine word.
union Held {
    int64_t integer;
    double real;
    int8_t boolean;
    GodotObject *owner;
};

// Every argument but the receiver, laid out where a ptrcall can read it.
// `first` is where the arguments proper begin: one for a method, since 14.4
// puts the receiver before them, and zero for a singleton's, which has none.
// `room` is NULL exactly where `method->boxed` is zero. False on a value that
// is not what the signature said -- 5.1: a wrong call stops rather than
// corrupts.
bool laid_out(const BoundMethod *method, const LhatValue *arguments,
              size_t first, size_t wanted, GDExtensionConstTypePtr *slots,
              Held *held, Boxed *room)
{
    const Godot *module = method->module;
    size_t built = 0;
    for (size_t i = 0; i < wanted; i++) {
        LhatValue given = arguments[i + first];
        uint8_t kind = method->arguments[i];
        if (kind >= LHAT_GD_HOSTDATA) {
            // 8.8: the handle holds the engine's own object, so the pointer
            // it keeps is already what a ptrcall wants -- nothing is copied.
            int which = kind - LHAT_GD_HOSTDATA;
            const LhatHostDataTag *tag =
                which == Variant::CALLABLE  ? module->callable_tag
                : which == Variant::SIGNAL  ? module->signal_tag
                                            : module->packed_tags[which];
            slots[i] = (GDExtensionConstTypePtr)lhat_hostdata_pointer(given,
                                                                      tag);
            if (slots[i] == nullptr) {
                return false;
            }
            continue;
        }
        if (kind >= LHAT_GD_HOSTVALUE) {
            // 8.9: the machine's bytes are the engine's layout, so this is
            // the whole conversion. The case the mathematical types take.
            slots[i] = (GDExtensionConstTypePtr)lhat_hostvalue_data(
                given, module->value_tags[kind - LHAT_GD_HOSTVALUE]);
            if (slots[i] == nullptr) {
                return false;
            }
            continue;
        }
        switch (kind) {
            case LHAT_GD_BOOL:
                held[i].boolean = lhat_as_bool(given) ? 1 : 0;
                slots[i] = &held[i].boolean;
                break;
            case LHAT_GD_INT:
                // 14.8's one number^ arrives either way round, and which
                // width the engine wanted is what the kind remembers.
                held[i].integer = lhat_is_integer(given)
                                      ? lhat_as_integer(given)
                                      : (int64_t)lhat_as_real(given);
                slots[i] = &held[i].integer;
                break;
            case LHAT_GD_FLOAT:
                held[i].real = lhat_is_integer(given)
                                   ? (double)lhat_as_integer(given)
                                   : lhat_as_real(given);
                slots[i] = &held[i].real;
                break;
            case LHAT_GD_OBJECT: {
                // What a ptrcall is handed is the place the pointer sits in,
                // not the pointer -- and nothing at all where there is no
                // object, which is how a null argument is spelt.
                held[i].owner = owner_of(given, module);
                slots[i] = held[i].owner != nullptr
                               ? (GDExtensionConstTypePtr)&held[i].owner
                               : nullptr;
                break;
            }
            default: {
                // The four that are built. They share one counter, so the
                // frame is as wide as the widest signature rather than as
                // wide as the argument list.
                if (room == nullptr || built >= LHAT_GD_MAX_BOXED) {
                    return false;
                }
                size_t at = built++;
                if (kind == LHAT_GD_VARIANT) {
                    room->variants[at] = to_variant(given, module);
                    slots[i] = &room->variants[at];
                    break;
                }
                if (!text_of(given, &room->texts[at])) {
                    return false;
                }
                if (kind == LHAT_GD_STRING) {
                    slots[i] = &room->texts[at];
                } else if (kind == LHAT_GD_STRINGNAME) {
                    room->names[at] = StringName(room->texts[at]);
                    slots[i] = &room->names[at];
                } else {
                    room->paths[at] = NodePath(room->texts[at]);
                    slots[i] = &room->paths[at];
                }
                break;
            }
        }
    }
    return true;
}

// The call itself, and what comes back read as the kind the signature said.
LhatValue answered(LhatMachine *machine, const BoundMethod *method,
                   GodotObject *owner, const GDExtensionConstTypePtr *slots)
{
    const Godot *module = method->module;
    GDExtensionMethodBindPtr bind = method->bind;
    switch (method->answer) {
        case LHAT_GD_NIL:
            internal::gdextension_interface_object_method_bind_ptrcall(
                bind, owner, slots, nullptr);
            return lhat_nil();
        case LHAT_GD_BOOL: {
            int8_t back = 0;
            internal::gdextension_interface_object_method_bind_ptrcall(
                bind, owner, slots, &back);
            return lhat_bool(back != 0);
        }
        case LHAT_GD_INT: {
            int64_t back = 0;
            internal::gdextension_interface_object_method_bind_ptrcall(
                bind, owner, slots, &back);
            return lhat_integer(back);
        }
        case LHAT_GD_FLOAT: {
            double back = 0;
            internal::gdextension_interface_object_method_bind_ptrcall(
                bind, owner, slots, &back);
            return lhat_real(back);
        }
        case LHAT_GD_STRING: {
            String back;
            internal::gdextension_interface_object_method_bind_ptrcall(
                bind, owner, slots, &back);
            return text_answer(machine, back);
        }
        case LHAT_GD_STRINGNAME: {
            StringName back;
            internal::gdextension_interface_object_method_bind_ptrcall(
                bind, owner, slots, &back);
            return text_answer(machine, String(back));
        }
        case LHAT_GD_NODEPATH: {
            NodePath back;
            internal::gdextension_interface_object_method_bind_ptrcall(
                bind, owner, slots, &back);
            return text_answer(machine, String(back));
        }
        case LHAT_GD_OBJECT: {
            GodotObject *back = nullptr;
            internal::gdextension_interface_object_method_bind_ptrcall(
                bind, owner, slots, &back);
            // The binding is what turns the engine's object back into one
            // godot-cpp holds -- the step _call_native_mb_ret_obj takes, and
            // the only way a ptrcall ever answers an object.
            Object *given =
                back != nullptr
                    ? reinterpret_cast<Object *>(
                          internal::get_object_instance_binding(back))
                    : nullptr;
            LhatValue made = lhat_nil();
            make_object(machine, module, given, &made);
            return made;
        }
        case LHAT_GD_VARIANT: {
            Variant back;
            internal::gdextension_interface_object_method_bind_ptrcall(
                bind, owner, slots, &back);
            LhatValue made = lhat_nil();
            from_variant(machine, back, &made, module);
            return made;
        }
        default: {
            // 8.9: the bytes come back into room wide enough for any value
            // type, and the tag is what says how many of them are the value.
            uint8_t room[LHAT_HOSTVALUE_MAX_BYTES] = {};
            internal::gdextension_interface_object_method_bind_ptrcall(
                bind, owner, slots, room);
            LhatValue made = lhat_nil();
            lhat_make_hostvalue(
                machine,
                module->value_tags[method->answer - LHAT_GD_HOSTVALUE], room,
                &made);
            return made;
        }
    }
}

// 05 の 8.7: one engine method, reached through the bind found when it was
// registered. What a call by name pays -- a String, a StringName, an Array,
// and a Variant for every argument, before ClassDB has even been asked -- is
// all paid ahead of the run here, and what crosses is the bytes the machine
// already holds (8.9).
//
// 14.4 hands the receiver first, which is what the signature's self^ says.
void bound_call(LhatMachine *machine, void *context,
                const LhatValue *arguments, size_t count,
                LhatValue *answers, int *answer_count)
{
    const BoundMethod *method = (const BoundMethod *)context;
    const Godot *module = method->module;

    // A singleton's method is written without a receiver, so the arguments
    // start where a method's receiver would have been.
    const bool alone = method->module_path != nullptr;
    const size_t first = alone ? 0 : 1;
    GodotObject *owner =
        alone ? method->owner
              : (count > 0 ? owner_of(arguments[0], module) : nullptr);
    if (owner == nullptr) {
        // A singleton the engine does not hold on this build -- a server that
        // is not compiled in -- reads as absent rather than as a crash.
        answers[0] = gone(method->name,
                    (!alone && count > 0) ? arguments[0] : lhat_nil(), module);
        *answer_count = 1;
        return;
    }
    // An engine that no longer answers to this hash, and an answer no ptrcall
    // shape covers. Neither happens to a table generated against the
    // godot-cpp being built with; the second is asked because a wider
    // godot-classes.txt could reach a method answering a Callable or a packed
    // array, and by name is the right answer there rather than no answer.
    // Only this road wants something with methods on it, so only this road
    // pays for the instance binding.
    if (method->bind == nullptr || method->answer >= LHAT_GD_HOSTDATA) {
        // This road wants something with methods on it, so this is the one
        // place that pays for godot-cpp's instance binding.
        Object *object =
            alone ? reinterpret_cast<Object *>(
                        internal::get_object_instance_binding(owner))
                  : object_of(arguments[0], module);
        if (object == nullptr) {
            answers[0] = gone(method->name, alone ? lhat_nil() : arguments[0],
                        module);
            *answer_count = 1;
            return;
        }
        answers[0] = by_name(machine, method, object, arguments, first, count);
        *answer_count = 1;
        return;
    }

    size_t wanted = method->arg_count;
    if (count < wanted + first || wanted > LHAT_GD_MAX_ARGS) {
        return;  // 5.1: a wrong call stops rather than corrupts
    }

    GDExtensionConstTypePtr slots[LHAT_GD_MAX_ARGS];
    Held held[LHAT_GD_MAX_ARGS];

    // The one branch that matters: a signature wanting nothing built puts
    // down no Boxed at all, which is what five methods in six do.
    if (method->boxed != 0) {
        Boxed room;
        if (!laid_out(method, arguments, first, wanted, slots, held, &room)) {
            return;
        }
        answers[0] = answered(machine, method, owner, slots);
        *answer_count = 1;
        return;
    }
    if (!laid_out(method, arguments, first, wanted, slots, held, nullptr)) {
        return;
    }
    answers[0] = answered(machine, method, owner, slots);
    *answer_count = 1;
}

const Godot *register_godot(LhatProgram *program)
{
    if (shared == nullptr) {
        shared = memnew(Godot);
    }
    Godot *module = shared;

    // 05 の 8.8改: the engine's tree, godot.Object at its root. Everything
    // below registers onto types this made, so it is first.
    if (!register_godot_classes(program, module)) {
        return nullptr;
    }
    const LhatHostDataTag *const *root = module->tags.getptr(StringName("Object"));
    module->object_tag = root != nullptr ? *root : nullptr;
    if (module->object_tag == nullptr) {
        return nullptr;
    }

    struct {
        const char *name;
        const char *signature;
        LhatHostFn call;
    } members[] = {
        // 02 の 14.15 with 14.11改: no way to make one out of nothing. Every
        // godot.Object here stands for an object the engine made, and the one
        // an override^ new is handed is where they all come from -- a value
        // meaning "no object" would be a wrapper whose isValid() is false by
        // construction, which is nothing a program has a use for.
        {"isValid", "f^self^ -> bool^;", godot_is_valid},
        {"className", "f^self^ -> string^;", godot_class_name},
        {"isClass", "f^self^, string^ -> bool^;", godot_is_class},
        // 8.8改: `get` and `set` are the engine's own and are generated onto
        // godot.Object with the rest of Object's methods, so nothing is
        // written for them here. `call` and `emit` are not: both are vararg,
        // which has no ptrcall, so they stay the calls by name they were.
        {"call", "f^self^, string^, ... -> any^;", godot_call},
        {"emit", "p^self^, string^, ...;", godot_emit},
        {"dispose", "p^self^;", godot_dispose},
    };
    // 02 の 18: what a script may write above a declaration. The engine acts
    // on these; L^ only carries them (18.1), which is why registering them is
    // all the language needs to know.
    struct {
        const char *name;
        uint32_t targets;
        const char *signature;
        const char *excludes;
        const char *needs;
    } annotations[] = {
        // Which def^ of this unit a node wears, and when it runs. Without a
        // mark the engine would need a rule of its own -- "the one public^
        // def^" was that rule, and it made a unit publishing a second class
        // unwearable for no reason the language has.
        //
        // Two marks rather than one because there are two answers, not
        // because there are two questions: @tool is @game plus the editor.
        // A unit writes at most one of them, and a unit publishing a single
        // class need write neither -- with one candidate there is nothing to
        // choose, and running in the editor is always chosen out loud.
        //
        // Public only: the engine reaches the class through the table the
        // unit answers with (05 の 5.5), so one kept private is not something
        // a node could wear however it is marked.
        //
        // FILEUNIQUE counts each name on its own, which catches a second
        // @game and a second @tool. That the two are one choice is 18.5.1's
        // exclusion, said here from both sides -- either would do,
        // and both is how the pair reads as a pair.
        {"game", LHAT_ANNOTATION_PUBLIC | LHAT_ANNOTATION_FILEUNIQUE, nullptr,
         "tool", nullptr},
        {"tool", LHAT_ANNOTATION_PUBLIC | LHAT_ANNOTATION_FILEUNIQUE, nullptr,
         "game", nullptr},
        // 05 の 3.2's opposite number: GDScript is anonymous unless it writes
        // class_name, and so is a .lh unless it writes this. Spelt with the
        // rest of the @export family, since what it does is what they do --
        // hand something of the writer's over to the engine, where nothing
        // unmarked goes.
        //
        // What the project registers is the binding's own name; there is no
        // argument, because a name written twice is a name that can disagree
        // with itself. One per file, which is all the engine keeps per path.
        {"export_class", LHAT_ANNOTATION_PUBLIC | LHAT_ANNOTATION_FILEUNIQUE,
         nullptr, nullptr, nullptr},
        // 18.5.2: an icon hangs on a global class name, so on the mark that
        // asks for one. Its targets are that mark's for the same reason --
        // a class kept private can never have a name for an icon to hang on,
        // and one file registers one name and so wears one icon.
        {"icon", LHAT_ANNOTATION_PUBLIC | LHAT_ANNOTATION_FILEUNIQUE,
         "p^ string^;", nullptr, "export_class"},
        {"export", LHAT_ANNOTATION_FIELD, nullptr, nullptr, nullptr},
        {"export_range", LHAT_ANNOTATION_FIELD, "p^ number^, number^, ...;",
         nullptr, nullptr},
        // An enum wants at least one name; a file filter is optional and
        // there may be several, which is what a bare variadic says.
        {"export_enum", LHAT_ANNOTATION_FIELD, "p^ string^, ...:string^;",
         nullptr, nullptr},
        {"export_file", LHAT_ANNOTATION_FIELD, "p^ ...:string^;", nullptr,
         nullptr},
        {"export_multiline", LHAT_ANNOTATION_FIELD, nullptr, nullptr,
         nullptr},
        // Written on the member that emits it: what the engine has to be
        // told is a name and an argument list, and a member already is one.
        {"signal", LHAT_ANNOTATION_MEMBER, nullptr, nullptr, nullptr},
        // The words are strings and the channel is a number, so the tail
        // takes no type -- and none of them is required, since every part of
        // the configuration has a default (_get_rpc_config).
        {"rpc", LHAT_ANNOTATION_MEMBER, "p^ ...;", nullptr, nullptr},
    };
    for (const auto &annotation : annotations) {
        if (!lhat_register_annotation(program, "godot", annotation.name,
                                      annotation.targets) ||
            (annotation.signature != nullptr &&
             !lhat_register_annotation_signature(program, annotation.name,
                                                 annotation.signature)) ||
            (annotation.excludes != nullptr &&
             !lhat_register_annotation_exclusive(program, annotation.name,
                                                 annotation.excludes)) ||
            (annotation.needs != nullptr &&
             !lhat_register_annotation_requisite(program, annotation.name,
                                                 annotation.needs))) {
            return nullptr;
        }
    }

    for (const auto &member : members) {
        if (!lhat_register_member(program, "godot", "Object", member.name,
                                  member.signature, member.call, module)) {
            return nullptr;
        }
    }

    // 02 の 18: @tool is additive -- a class marked with it runs while a scene
    // is being edited *and* when the game runs. So a writer needs to be able
    // to tell the two apart, the way GDScript's `if Engine.is_editor_hint():`
    // does. A subroutine of the module rather than a member of Object: it is
    // not about any object.
    if (!lhat_register_func(program, "godot", "isEditorHint", "f^ -> bool^;",
                            godot_is_editor_hint, module)) {
        return nullptr;
    }

    // 05 の 8.9: the mathematical types, which are values rather than objects
    // and so nothing the one hostdata type above could stand for.
    if (!register_values(program, module)) {
        return nullptr;
    }

    // 05 の 8.8: the two that look like values and are not, and the ten runs
    // of them that are copied on write.
    if (!register_handles(program, module) ||
        !register_packed(program, module)) {
        return nullptr;
    }

    // 05 の 8.7: what the engine itself answers to, bound ahead of the run.
    // Last, because every type a signature there names is registered above --
    // the classes at the top of this function and the value types just now.
    if (!register_godot_api(program, module)) {
        return nullptr;
    }
    return module;
}

// 05 の 8.8改: which of the registered classes an object crosses as. Its own
// class where that was registered, and the nearest ancestor that was
// otherwise -- godot-classes.txt names a handful and the engine has 971, so
// most objects arrive as something further up.
//
// The answer is written back under the class that was asked about, so the
// walk up ClassDB happens once per class met rather than once per value.
const LhatHostDataTag *tag_for(const Godot *module, Object *object)
{
    if (object == nullptr) {
        return module->object_tag;
    }
    StringName named = object->get_class();
    for (StringName walk = named; !walk.is_empty();
         walk = ClassDBSingleton::get_singleton()->get_parent_class(walk)) {
        const LhatHostDataTag *const *found = module->tags.getptr(walk);
        if (found != nullptr) {
            if (walk != named) {
                module->tags.insert(named, *found);
            }
            return *found;
        }
    }
    module->tags.insert(named, module->object_tag);
    return module->object_tag;
}

bool make_object(LhatMachine *machine, const Godot *module, Object *object,
                 LhatValue *out)
{
    if (module == nullptr) {
        return false;
    }
    Handle *handle = memnew(Handle);
    if (object != nullptr) {
        handle->id = object->get_instance_id();
        // Nothing else would keep a RefCounted alive while L^ holds it; a
        // Node belongs to the tree and is not held here on purpose.
        RefCounted *counted = Object::cast_to<RefCounted>(object);
        if (counted != nullptr) {
            handle->hold = Ref<RefCounted>(counted);
        }
    }
    if (!lhat_machine_make_hostdata(machine, tag_for(module, object), handle,
                                    out)) {
        memdelete(handle);
        return false;
    }
    return true;
}

Object *object_of(LhatValue value, const Godot *module)
{
    if (module == nullptr) {
        return nullptr;
    }
    return resolve(
        (const Handle *)lhat_hostdata_pointer(value, module->object_tag));
}

bool make_signal_emitter(LhatMachine *machine, const SignalEmitter *emitter,
                         uint8_t parameters, bool variadic, LhatValue *out)
{
    if (machine == nullptr || emitter == nullptr) {
        return false;
    }
    // 14.4: reached through an instance, so it is handed a receiver -- which
    // is what makes it stand in for the member it replaces rather than for a
    // static one. Nothing is registered under an overloaded name here, so the
    // parameter types a search would ask for are left unbuilt (14.12).
    return lhat_machine_make_host(machine, signal_emit, (void *)emitter,
                                  parameters, variadic, /*takes_self=*/true,
                                  /*self_last=*/false, nullptr, out);
}

}  // namespace host
}  // namespace godot
