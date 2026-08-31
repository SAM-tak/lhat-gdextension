#include "lhat_instance.h"

#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/script_language.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/local_vector.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/signal.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "lhat.h"
#include "lhat_host.h"
#include "lhat_language.h"
#include "lhat_script.h"
#include "lhat_godot_values.h"
#include "lhat_variant.h"

namespace godot {

namespace {

// One node's worth. The instance itself lives on the machine and is rooted
// in the script's table; what is here is the pairing, and the id that undoes
// it when the node goes.
struct Instance {
    Object *owner = nullptr;
    Ref<LhatScript> script;
    LhatValue self = lhat_nil();
    int64_t id = 0;
    // Which of the script's machines `self` came from. _reload disposes it,
    // and the node holding this goes on living -- so past that point `self`
    // points into freed memory.
    uint64_t born = 0;
    // 18: whether the @node fields have been filled. Done once, on the way
    // into the first call that finds the node in the tree.
    bool dressed = false;
};

// 15.5 with GDScript's await: a body that wrote yield^ answers a coroutine
// rather than running, and something has to advance it. That something is
// the engine -- the signal it is waiting on is the engine's -- so what a
// yield^ hands over is a godot.Signal and what comes back is one resume.
//
// The way back in is a method on the node itself rather than an object of
// ours: a Callable holds an object id and no reference, so a helper made for
// the occasion would be collected before the signal ever fired, while the
// node is exactly as alive as the instance whose body is waiting.
//
// The park number rides in the method's own name rather than as a bind,
// because Godot compares two bound Callables by the callable and the
// *number* of binds and not their values (callable_bind.cpp's _equal_func)
// -- so two bodies of one node waiting on one signal would look like the
// same connection, and the second would be refused. Whatever arguments the
// signal itself carries are ignored, which is why any signal will do.
const char *resume_prefix() { return "_lhat_resume#"; }

StringName resume_name(int64_t id)
{
    return StringName(String(resume_prefix()) + String::num_int64(id));
}

// Whether `name` is one of those, and which park it names.
bool resume_id(const StringName &name, int64_t *id)
{
    const String spelt = String(name);
    const String prefix = String(resume_prefix());
    if (!spelt.begins_with(prefix)) {
        return false;
    }
    const String digits = spelt.substr(prefix.length());
    if (digits.is_empty() || !digits.is_valid_int()) {
        return false;
    }
    *id = digits.to_int();
    return true;
}

Instance *of(GDExtensionScriptInstanceDataPtr data)
{
    return (Instance *)data;
}

// 09 の 3.2: what a debugger asks for by frame. The frame answers self^ as a
// value, and the panel wants the ScriptInstance the engine knows -- so the
// two are paired here, keyed by the table self^ points at.
//
// Safe as a key because the collector marks and sweeps and never moves: a
// live table stays where it was made. The pairing is undone in instance_free,
// which is the only way an entry can outlive what it names.
HashMap<const void *, GDExtensionScriptInstanceDataPtr> &instances_by_self()
{
    static HashMap<const void *, GDExtensionScriptInstanceDataPtr> it;
    return it;
}

// Whether `self` is still worth reading. Asked before anything looks at it,
// including the "is it a table" check, because that check reads the memory it
// is asking about -- once the machine is gone, so is the right to look.
//
// A node whose script was reloaded under it answers nothing rather than
// answering wrongly. It is not mended in place: 03 の 5.3 has a compile
// answer fresh modules, and the fields this instance was holding were the old
// ones. What brings the node back is being given the script again, which
// reload_from_disk does for everything wearing it -- and where a reload came
// from somewhere else, opening the scene again.
bool living(const Instance *it)
{
    return it->script.is_valid() && it->script->lhat_machine() != nullptr &&
           it->script->lhat_generation() == it->born &&
           lhat_is_object_kind(it->self, LHAT_OBJECT_TABLE);
}

const StringName &name_of(GDExtensionConstStringNamePtr from)
{
    return *reinterpret_cast<const StringName *>(from);
}

// ---------------------------------------------------------------------------
// Properties: an instance's self^ fields, read and written by name

// 14.3 fixes an instance's fields when it is made, so a name that is not
// one of them is not this instance's to answer for -- the engine asks about
// every name it can think of and reads `false` as "not mine". A field
// holding nil^ has no entry in the table, though, and "no entry" is not
// "not mine": the declaration (14.16) is what tells those two apart.
bool declares(const Instance *it, const LhatTable *table, LhatValue key,
              const StringName &name)
{
    return !lhat_is_nil(lhat_table_get(table, key)) ||
           it->script->lhat_declared_type(String(name)) != nullptr;
}

GDExtensionBool instance_set(GDExtensionScriptInstanceDataPtr data,
                             GDExtensionConstStringNamePtr name,
                             GDExtensionConstVariantPtr value)
{
    Instance *it = of(data);
    if (!living(it)) {
        return false;
    }
    LhatMachine *machine = it->script->lhat_machine();

    CharString spelt = String(name_of(name)).utf8();
    LhatValue key = lhat_nil();
    LhatValue held = lhat_nil();
    if (!lhat_machine_make_string(machine, spelt.get_data(),
                                  (size_t)spelt.length(), &key)) {
        return false;
    }
    LhatTable *table = (LhatTable *)lhat_as_object(it->self);
    if (!declares(it, table, key, name_of(name))) {
        return false;
    }
    // 05 の 8.9: a field holding a box is written through the box rather
    // than over it -- the box is the thing the field holds, and what changes
    // is the bytes inside. Nothing here could make a new one anyway: the
    // heap is the machine's and box^ is the spelling that reaches it.
    const Variant &given = *reinterpret_cast<const Variant *>(value);
    LhatValue standing = lhat_table_get(table, key);
    if (host::box_takes_variant(standing, it->script->godot(), given)) {
        return true;
    }
    if (!host::from_variant(machine, given, &held, it->script->godot())) {
        return false;
    }
    bool refused = false;
    return lhat_machine_table_set(machine, table, key, held, &refused) &&
           !refused;
}

GDExtensionBool instance_get(GDExtensionScriptInstanceDataPtr data,
                             GDExtensionConstStringNamePtr name,
                             GDExtensionVariantPtr answer)
{
    Instance *it = of(data);
    if (!living(it)) {
        return false;
    }
    LhatMachine *machine = it->script->lhat_machine();

    CharString spelt = String(name_of(name)).utf8();
    LhatValue key = lhat_nil();
    if (!lhat_machine_make_string(machine, spelt.get_data(),
                                  (size_t)spelt.length(), &key)) {
        return false;
    }
    const LhatTable *table = (const LhatTable *)lhat_as_object(it->self);
    if (!declares(it, table, key, name_of(name))) {
        return false;
    }
    // A declared field holding nil^ answers nil, which is what a .tscn
    // writes back and what the inspector draws an empty slot from.
    *reinterpret_cast<Variant *>(answer) =
        host::to_variant(lhat_table_get(table, key), it->script->godot());
    return true;
}

// The arguments as one comma-joined string, which is the shape Godot reads a
// hint in. A name (18.3) is spelt out the same as a string: what it meant was
// never the language's to say.
String joined(LhatAnnotation at)
{
    String out;
    for (size_t i = 0; i < at.argument_count; i++) {
        LhatAnnotationArgument argument = lhat_annotation_argument(at, i);
        if (argument.text == NULL) {
            continue;
        }
        if (!out.is_empty()) {
            out += String(",");
        }
        out += String::utf8(argument.text, (int)argument.length);
    }
    return out;
}

// 02 の 18: a field the writer marked. What @export means is Godot's, and
// the language only carried it here -- so this is where the meaning is put
// back on, and nowhere else.
// What Godot draws a field with. Coarse on purpose -- 18.3 gives a host this
// much of a type and no more, and picking a spin box from a text field is all
// that rides on it here.
void typed_as(LhatUnitTypeKind kind, Dictionary *info)
{
    switch (kind) {
        case LHAT_UNIT_TYPE_NUMBER:
            (*info)["type"] = Variant::FLOAT;
            return;
        case LHAT_UNIT_TYPE_STRING:
            (*info)["type"] = Variant::STRING;
            return;
        case LHAT_UNIT_TYPE_BOOL:
            (*info)["type"] = Variant::BOOL;
            return;
        default:
            break;
    }
    // Anything else is shown as it comes. NIL alone would read as "no
    // property"; the usage bit is what makes it "any value".
    (*info)["type"] = Variant::NIL;
    (*info)["usage"] = PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SCRIPT_VARIABLE |
                       PROPERTY_USAGE_NIL_IS_VARIANT;
}

// 02 の 14.16's spelling, read as a shape the inspector can draw. What the
// field holds is the better answer where it holds anything (14.11 ran the
// initialiser), so this is for the two places that leaves: a field holding
// nil^, which has no value to read a type off at all, and an object field,
// whose Variant says only OBJECT while the declaration says which class.
struct Shape {
    Variant::Type type = Variant::NIL;
    uint32_t hint = PROPERTY_HINT_NONE;
    String hint_string;
    StringName class_name;
};

// 11.7: a field that may hold nothing is written `T|nil^`, and it is T the
// engine has to be told about. A union of several real arms says nothing one
// property can draw, so only the one-armed case answers.
const LhatRuntimeType *without_nil(const LhatRuntimeType *said)
{
    if (said == nullptr || said->kind != LHAT_TYPE_RT_UNION) {
        return said;
    }
    const LhatRuntimeType *only = nullptr;
    for (size_t i = 0; i < said->part_count; i++) {
        const LhatRuntimeType *arm = said->parts[i];
        if (arm == nullptr || arm->kind == LHAT_TYPE_RT_NIL) {
            continue;
        }
        if (only != nullptr) {
            return nullptr;  // two ways to fill it; the engine has one slot
        }
        only = arm;
    }
    return only;
}

// The name a host type was registered under, without its module.
String host_named(const char *module, const char *name)
{
    (void)module;  // every one of ours is "godot"; the name is what differs
    return name != nullptr ? String::utf8(name) : String();
}

bool shape_of(const LhatRuntimeType *declared, Shape *out)
{
    const LhatRuntimeType *said = without_nil(declared);
    if (said == nullptr) {
        return false;
    }
    switch (said->kind) {
        case LHAT_TYPE_RT_NUMBER:
            out->type = Variant::FLOAT;
            return true;
        case LHAT_TYPE_RT_STRING:
            out->type = Variant::STRING;
            return true;
        case LHAT_TYPE_RT_BOOL:
            out->type = Variant::BOOL;
            return true;
        case LHAT_TYPE_RT_HOSTVALUE:
        case LHAT_TYPE_RT_HOSTVALUE_BOX: {
            // 05 の 8.9: a host value cannot live in a table, so a field
            // holding one holds T.Box^ instead -- and to_variant unwraps a
            // box while box_takes_variant writes back through it, so as far
            // as the inspector is concerned both are the value.
            if (said->hostvalue_tag == nullptr) {
                return false;
            }
            const String bare = host_named(said->hostvalue_tag->module,
                                           said->hostvalue_tag->name);
            for (int kind = 1; kind < Variant::VARIANT_MAX; kind++) {
                if (Variant::get_type_name((Variant::Type)kind) == bare) {
                    out->type = (Variant::Type)kind;
                    return true;
                }
            }
            return false;
        }
        case LHAT_TYPE_RT_HOSTDATA: {
            if (said->hostdata_tag == nullptr) {
                return false;
            }
            const String bare = host_named(said->hostdata_tag->module,
                                           said->hostdata_tag->name);
            // Callable, Signal and the packed arrays are handles of ours and
            // Variant kinds of the engine's; the classes are objects.
            for (int kind = 1; kind < Variant::VARIANT_MAX; kind++) {
                if (Variant::get_type_name((Variant::Type)kind) == bare) {
                    out->type = (Variant::Type)kind;
                    return true;
                }
            }
            if (bare.begins_with("ArrayOf")) {
                out->type = Variant::ARRAY;  // 8.8's typed array, as an Array
                return true;
            }
            if (!ClassDB::class_exists(bare)) {
                return false;
            }
            out->type = Variant::OBJECT;
            out->class_name = StringName(bare);
            out->hint_string = bare;
            // Which of the two pickers the inspector puts on it. A class
            // that is neither -- an Object of its own -- gets the class name
            // and no picker, which is what the engine does with one too.
            ClassDBSingleton *db = ClassDBSingleton::get_singleton();
            if (db != nullptr && db->is_parent_class(bare, "Resource")) {
                out->hint = PROPERTY_HINT_RESOURCE_TYPE;
            } else if (db != nullptr && db->is_parent_class(bare, "Node")) {
                out->hint = PROPERTY_HINT_NODE_TYPE;
            } else {
                out->hint_string = String();
            }
            return true;
        }
        default:
            return false;  // a table, a signature, a type of the writer's own
    }
}

// What a field holds, found by name rather than by a key made for the
// occasion -- a property list is asked for often and a made string would be
// heap the collector then has to walk. An instance's fields are a handful,
// so the scan costs less than the allocation would.
LhatValue held_named(const LhatTable *fields, const String &name)
{
    if (fields == nullptr) {
        return lhat_nil();
    }
    const CharString spelt = name.utf8();
    for (size_t i = 0; i < fields->entry_capacity; i++) {
        const LhatTableEntry *entry = &fields->entries[i];
        if (!lhat_is_object_kind(entry->key, LHAT_OBJECT_STRING)) {
            continue;
        }
        const LhatString *key = (const LhatString *)lhat_as_object(entry->key);
        if (key->length == (size_t)spelt.length() &&
            memcmp(key->text, spelt.get_data(), key->length) == 0) {
            return entry->value;
        }
    }
    return lhat_nil();
}

// The three sources settled in one place, so that the live instance's list
// and the declared one cannot drift apart. `held` is what the field holds
// now, NIL where it holds nothing.
void drawn_as(const LhatScript *script, const String &field,
              LhatUnitTypeKind kind, const Variant &held, Variant::Type *type,
              StringName *class_name, uint32_t *hint, String *hint_string,
              uint32_t *usage)
{
    Shape shape;
    const bool known =
        script != nullptr &&
        shape_of(script->lhat_declared_type(field), &shape);

    *type = held.get_type();
    if (*type == Variant::NIL && known) {
        *type = shape.type;
    }
    if (*type == Variant::NIL) {
        // 18.3's four coarse kinds, which is all that is left to go on.
        switch (kind) {
            case LHAT_UNIT_TYPE_NUMBER: *type = Variant::FLOAT; break;
            case LHAT_UNIT_TYPE_STRING: *type = Variant::STRING; break;
            case LHAT_UNIT_TYPE_BOOL:   *type = Variant::BOOL; break;
            default: break;
        }
    }
    if (*type == Variant::NIL) {
        // NIL alone reads as "no property" and the engine drops the row.
        *usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
    }
    if (known && *type == shape.type) {
        if (!shape.class_name.is_empty()) {
            *class_name = shape.class_name;
        }
        // 18 の @export_range and friends were written by hand and win: the
        // declaration only fills a hint nothing else gave.
        if (*hint == PROPERTY_HINT_NONE && shape.hint != PROPERTY_HINT_NONE) {
            *hint = shape.hint;
            *hint_string = shape.hint_string;
        }
    }
}

bool exported_as(const LhatScript *script, const String &field,
                 String *hint_string, uint32_t *hint)
{
    const LhatUnit *unit = script->lhat_unit();
    if (unit == nullptr) {
        return false;
    }
    CharString klass = script->lhat_class_name().utf8();
    CharString name = field.utf8();
    size_t written =
        lhat_unit_annotation_count(unit, klass.get_data(), name.get_data());

    for (size_t i = 0; i < written; i++) {
        LhatAnnotation at = lhat_unit_annotation(unit, klass.get_data(),
                                                 name.get_data(), i);
        String spelt = String::utf8(at.name, (int)at.name_length);
        if (spelt == "export") {
            *hint = PROPERTY_HINT_NONE;
            *hint_string = String();
            return true;
        }
        if (spelt == "export_range" && at.argument_count >= 2) {
            *hint = PROPERTY_HINT_RANGE;
            *hint_string =
                String::num(lhat_annotation_argument(at, 0).number) +
                String(",") +
                String::num(lhat_annotation_argument(at, 1).number);
            return true;
        }
        if (spelt == "export_multiline") {
            *hint = PROPERTY_HINT_MULTILINE_TEXT;
            *hint_string = String();
            return true;
        }
        // Both of these are a comma-joined list to the engine, and both take
        // their pieces as written -- 18.3 keeps an argument a literal, so
        // there is nothing to resolve on the way.
        if (spelt == "export_enum" && at.argument_count >= 1) {
            *hint = PROPERTY_HINT_ENUM;
            *hint_string = joined(at);
            return true;
        }
        if (spelt == "export_file") {
            // No filter means any file, which is what an empty hint says.
            *hint = PROPERTY_HINT_FILE;
            *hint_string = joined(at);
            return true;
        }
    }
    return false;
}

const GDExtensionPropertyInfo *instance_property_list(
    GDExtensionScriptInstanceDataPtr data, uint32_t *count)
{
    Instance *it = of(data);
    *count = 0;
    if (!living(it)) {
        return nullptr;
    }

    // 14.3 fixes an instance's fields when it is made, so what the writer
    // declared is the list -- and 18 says which of them the engine is to
    // show. Walked off the tree rather than off the instance: a field
    // holding nil^ has no entry in the table at all, and leaving it out
    // here is what made an @export of a godot.PackedScene disappear.
    const LhatUnit *unit = it->script->lhat_unit();
    if (unit == nullptr) {
        return nullptr;
    }
    const LhatTable *fields = (const LhatTable *)lhat_as_object(it->self);
    CharString klass = it->script->lhat_class_name().utf8();
    LocalVector<GDExtensionPropertyInfo> found;
    size_t declares = lhat_unit_member_count(unit, klass.get_data());
    for (size_t i = 0; i < declares; i++) {
        LhatUnitMember member = lhat_unit_member(unit, klass.get_data(), i);
        if (member.name == NULL) {
            continue;
        }
        String name = String::utf8(member.name, (int)member.name_length);

        String hint_string;
        uint32_t hint = PROPERTY_HINT_NONE;
        if (!exported_as(it->script.ptr(), name, &hint_string, &hint)) {
            continue;
        }

        // The type is what the field holds now, where it holds anything --
        // 14.11 ran its initialiser. A field holding nil^ has nothing to
        // read one off, which is what the declaration is for.
        Variant held = host::to_variant(held_named(fields, name),
                                        it->script->godot());
        Variant::Type type = Variant::NIL;
        StringName class_name;
        uint32_t usage =
            PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SCRIPT_VARIABLE;
        drawn_as(it->script.ptr(), name, member.type, held, &type,
                 &class_name, &hint, &hint_string, &usage);

        GDExtensionPropertyInfo info;
        info.type = (GDExtensionVariantType)type;
        info.name = memnew(StringName(name));
        info.class_name = memnew(StringName(class_name));
        info.hint = hint;
        info.hint_string = memnew(String(hint_string));
        info.usage = usage;
        found.push_back(info);
    }
    if (found.is_empty()) {
        return nullptr;
    }

    GDExtensionPropertyInfo *list = memnew_arr(GDExtensionPropertyInfo,
                                               found.size());
    for (uint32_t i = 0; i < found.size(); i++) {
        list[i] = found[i];
    }
    *count = (uint32_t)found.size();
    return list;
}

void instance_free_property_list(GDExtensionScriptInstanceDataPtr data,
                                 const GDExtensionPropertyInfo *list,
                                 uint32_t count)
{
    (void)data;
    if (list == NULL) {
        return;
    }
    for (uint32_t i = 0; i < count; i++) {
        memdelete((StringName *)list[i].name);
        memdelete((StringName *)list[i].class_name);
        memdelete((String *)list[i].hint_string);
    }
    memdelete_arr((GDExtensionPropertyInfo *)list);
}

GDExtensionVariantType instance_property_type(
    GDExtensionScriptInstanceDataPtr data, GDExtensionConstStringNamePtr name,
    GDExtensionBool *is_valid)
{
    (void)data;
    (void)name;
    *is_valid = false;
    return GDEXTENSION_VARIANT_TYPE_NIL;
}

GDExtensionBool instance_validate_property(
    GDExtensionScriptInstanceDataPtr data, GDExtensionPropertyInfo *property)
{
    (void)data;
    (void)property;
    return false;
}

GDExtensionBool instance_property_can_revert(
    GDExtensionScriptInstanceDataPtr data, GDExtensionConstStringNamePtr name)
{
    (void)data;
    (void)name;
    return false;
}

GDExtensionBool instance_property_get_revert(
    GDExtensionScriptInstanceDataPtr data, GDExtensionConstStringNamePtr name,
    GDExtensionVariantPtr answer)
{
    (void)data;
    (void)name;
    (void)answer;
    return false;
}

void instance_property_state(GDExtensionScriptInstanceDataPtr data,
                             GDExtensionScriptInstancePropertyStateAdd add,
                             void *userdata)
{
    (void)data;
    (void)add;
    (void)userdata;
}

// ---------------------------------------------------------------------------
// Methods

const GDExtensionMethodInfo *instance_method_list(
    GDExtensionScriptInstanceDataPtr data, uint32_t *count)
{
    (void)data;
    *count = 0;
    return nullptr;
}

void instance_free_method_list(GDExtensionScriptInstanceDataPtr data,
                               const GDExtensionMethodInfo *list,
                               uint32_t count)
{
    (void)data;
    (void)list;
    (void)count;
}

// What Node reads before it turns processing on, so this is what makes a
// unit that wrote _process get one.
GDExtensionBool instance_has_method(GDExtensionScriptInstanceDataPtr data,
                                    GDExtensionConstStringNamePtr name)
{
    const Instance *it = of(data);
    // Answering yes here is what turns processing on, so an instance left
    // behind by a reload says no and is called no further.
    if (!living(it)) {
        return false;
    }
    // The way a suspended body is resumed is a call on the node, so the
    // Callable a signal holds has to find something here.
    int64_t waiting = 0;
    return resume_id(name_of(name), &waiting) ||
           it->script->has_lhat_method(name_of(name));
}

GDExtensionInt instance_method_argument_count(
    GDExtensionScriptInstanceDataPtr data, GDExtensionConstStringNamePtr name,
    GDExtensionBool *is_valid)
{
    (void)data;
    (void)name;
    *is_valid = false;
    return 0;
}

// 18: what @node("path") named on a field, or empty.
String node_path_of(const LhatScript *script, const String &field)
{
    const LhatUnit *unit = script->lhat_unit();
    if (unit == nullptr) {
        return String();
    }
    CharString klass = script->lhat_class_name().utf8();
    CharString name = field.utf8();
    size_t written =
        lhat_unit_annotation_count(unit, klass.get_data(), name.get_data());
    for (size_t i = 0; i < written; i++) {
        LhatAnnotation at = lhat_unit_annotation(unit, klass.get_data(),
                                                 name.get_data(), i);
        if (String::utf8(at.name, (int)at.name_length) != "node") {
            continue;
        }
        LhatAnnotationArgument said = lhat_annotation_argument(at, 0);
        if (said.text != NULL) {
            return String::utf8(said.text, (int)said.length);
        }
    }
    return String();
}

// GDScript's @onready, done on the way into a call rather than at a
// notification for the reason @onready exists the other way round: get_node
// answers nothing until the node is in the tree, and the engine hands a
// script's own NOTIFICATION_READY to it only after Node::_notification has
// already called _ready (object.cpp's _notification_forward). Filling here
// is what puts the fields in place before any body of the writer's runs --
// _enter_tree's included, which @onready cannot manage.
//
// Once per instance. A node taken out of the tree and put back keeps what it
// was given, which is what @onready does too.
void dress_node_fields(Instance *it)
{
    const LhatScript *script = it->script.ptr();
    const LhatUnit *unit = script->lhat_unit();
    Node *owner = Object::cast_to<Node>(it->owner);
    if (unit == nullptr || !lhat_is_object_kind(it->self, LHAT_OBJECT_TABLE)) {
        it->dressed = true;
        return;
    }
    if (owner == nullptr) {
        it->dressed = true;  // not a node; there is nothing to reach through
        return;
    }
    // A path is asked of the node itself, not of the tree, so a child put
    // there by the scene answers before the parent has entered anything --
    // which is what $Foo does in GDScript, and what a mob told to initialize
    // itself before add_child needs. Where a path answers nothing, the tree
    // says whether that is early or wrong: outside it the next call tries
    // again, inside it there is nothing further to wait for.
    const bool settled = owner->is_inside_tree();
    bool all = true;
    LhatMachine *machine = it->script->lhat_machine();
    LhatTable *table = (LhatTable *)lhat_as_object(it->self);
    CharString klass = script->lhat_class_name().utf8();
    size_t count = lhat_unit_member_count(unit, klass.get_data());
    for (size_t i = 0; i < count; i++) {
        LhatUnitMember member = lhat_unit_member(unit, klass.get_data(), i);
        if (member.name == NULL) {
            continue;
        }
        String field = String::utf8(member.name, (int)member.name_length);
        String path = node_path_of(script, field);
        if (path.is_empty()) {
            continue;
        }

        Node *found = owner->get_node_or_null(NodePath(path));
        if (found == nullptr) {
            if (!settled) {
                all = false;  // the scene may not have finished building
                continue;
            }
            UtilityFunctions::push_error(host::problem(
                script->get_path(),
                String("@node(\"") + path + "\") on " + field +
                    " names no node under " + String(owner->get_name())));
            continue;
        }
        // The declaration is a promise about what stands there. Saying so
        // here is the difference between a wrong scene and a nil field the
        // writer has to work backwards from.
        Shape shape;
        if (shape_of(script->lhat_declared_type(field), &shape) &&
            shape.type == Variant::OBJECT && !shape.class_name.is_empty() &&
            !found->is_class(String(shape.class_name))) {
            UtilityFunctions::push_error(host::problem(
                script->get_path(),
                String("@node(\"") + path + "\") on " + field + " wants a " +
                    String(shape.class_name) + ", and what is there is a " +
                    found->get_class()));
            continue;
        }

        LhatValue key = lhat_nil();
        LhatValue held = lhat_nil();
        CharString spelt = field.utf8();
        if (!lhat_machine_make_string(machine, spelt.get_data(),
                                      (size_t)spelt.length(), &key) ||
            !host::from_variant(machine, Variant(found), &held,
                                script->godot())) {
            continue;
        }
        bool refused = false;
        lhat_machine_table_set(machine, table, key, held, &refused);
    }
    it->dressed = all;
}

void drive_await(Instance *it, int64_t id)
{
    LhatMachine *machine = it->script->lhat_machine();
    LhatValue coroutine = it->script->parked_coroutine(id);
    if (machine == nullptr || lhat_is_nil(coroutine)) {
        return;  // dropped by a reload, or never parked
    }

    LhatRunResult ran = lhat_machine_resume(machine, coroutine, nullptr, 0);
    if (ran.status != LHAT_RUN_OK) {
        UtilityFunctions::push_error(
            host::run_problem(machine, it->script->get_path(), ran));
        it->script->drop_coroutine(id);
        return;
    }
    if (lhat_machine_coroutine_done(coroutine)) {
        it->script->drop_coroutine(id);
        return;
    }

    // 15.6改 tells a yield apart from an end, and this is the yield: what it
    // handed over is what the body is waiting for.
    const Variant said = host::to_variant(ran.value, it->script->godot());
    if (said.get_type() != Variant::SIGNAL) {
        UtilityFunctions::push_error(host::problem(
            it->script->get_path(),
            String("a yield^ the engine is driving has to hand over a "
                   "godot.Signal to wait on; this one handed over a ") +
                Variant::get_type_name(said.get_type())));
        it->script->drop_coroutine(id);
        return;
    }

    // CONNECT_ONE_SHOT: one signal, one resume, and the connection undoes
    // itself -- so a body waiting on a timer does not wake to every later
    // tick of it.
    Signal waited = said;
    Callable back = Callable(it->owner, resume_name(id));
    if (waited.connect(back, Object::CONNECT_ONE_SHOT) != OK) {
        UtilityFunctions::push_error(host::problem(
            it->script->get_path(),
            String("could not wait on ") + waited.get_name()));
        it->script->drop_coroutine(id);
    }
}

// 02 の 14.4 の host 版: the receiver is this node's instance, and the member
// is looked up through its definition the way a written call looks it up.
void instance_call(GDExtensionScriptInstanceDataPtr data,
                   GDExtensionConstStringNamePtr method,
                   const GDExtensionConstVariantPtr *arguments,
                   GDExtensionInt count, GDExtensionVariantPtr answer,
                   GDExtensionCallError *error)
{
    Instance *it = of(data);
    *reinterpret_cast<Variant *>(answer) = Variant();

    // 05 の 3.2: EditorScript::run reaches an instance by the name _run, and
    // for a unit that named no module^ the body is what that stands for.
    // Asked before living(), which wants a self^ this instance has none of.
    if (it->script.is_valid() && it->script->is_editor_script()) {
        if (name_of(method) != StringName("_run")) {
            error->error = GDEXTENSION_CALL_ERROR_INVALID_METHOD;
            return;
        }
        error->error = it->script->run_body()
                           ? GDEXTENSION_CALL_OK
                           : GDEXTENSION_CALL_ERROR_INVALID_METHOD;
        return;
    }

    // A signal this instance's suspended body was waiting on has fired.
    // Whatever the signal passed is not this body's to read -- what it was
    // waiting for is that the signal happened.
    int64_t waiting = 0;
    if (resume_id(name_of(method), &waiting)) {
        error->error = GDEXTENSION_CALL_OK;
        if (living(it)) {
            drive_await(it, waiting);
        }
        return;
    }

    if (!living(it) || !it->script->has_lhat_method(name_of(method))) {
        error->error = GDEXTENSION_CALL_ERROR_INVALID_METHOD;
        return;
    }
    if (!it->dressed) {
        dress_node_fields(it);
    }
    LhatMachine *machine = it->script->lhat_machine();

    // Every argument is converted before anything runs: a collection happens
    // inside the interpreter loop, and what is built here is reachable only
    // once it is handed over (see lhat_variant.h).
    LocalVector<LhatValue> converted;
    // 05 の 8.9改: a host value is wider than a slot, so it is built in room
    // of the caller's rather than in the machine's single answer scratch --
    // one room per argument, all of them standing until the call returns,
    // which is what lets a member take two Vector3s.
    LocalVector<LhatHostValueRoom> rooms;
    rooms.resize((uint32_t)count);
    for (GDExtensionInt i = 0; i < count; i++) {
        const Variant &given = *reinterpret_cast<const Variant *>(arguments[i]);
        LhatValue held = lhat_nil();
        if (!host::from_variant(machine, given, &held,
                                it->script->godot())) {
            // from_variant leaves the value types alone, because what it
            // makes may be written into a table or an any^ and 8.9 bars one
            // from either. An argument is neither -- it is a place in the
            // frame about to be entered, which is exactly where a host value
            // is allowed to be.
            bool valued = false;
            held = host::value_placed_from_variant(
                it->script->godot(), given, &rooms[(uint32_t)i], &valued);
            if (!valued) {
                error->error = GDEXTENSION_CALL_ERROR_INVALID_ARGUMENT;
                error->argument = (int32_t)i;
                error->expected = 0;
                return;
            }
        }
        converted.push_back(held);
    }

    CharString spelt = String(name_of(method)).utf8();
    LhatRunResult ran = lhat_machine_call_member(
        machine, it->self, spelt.get_data(), (size_t)spelt.length(),
        converted.ptr(), converted.size());
    if (ran.status == LHAT_RUN_ARITY) {
        error->error = GDEXTENSION_CALL_ERROR_TOO_FEW_ARGUMENTS;
        error->argument = (int32_t)count;
        error->expected = (int32_t)count;
        return;
    }
    if (ran.status != LHAT_RUN_OK) {
        // 04 の 11.6改: line, panic value, and the frames still standing.
        // Pushing it is also what makes the engine ask for the backtrace
        // (ScriptBacktrace reads debug_get_current_stack_info), so the L^
        // frames land beside the caller's own.
        UtilityFunctions::push_error(host::run_problem(
            machine, it->script->get_path() + String(".") +
                         String(name_of(method)),
            ran));
        // And the call itself answered: the method was there, it ran, and it
        // failed. Saying INVALID_METHOD instead made the engine report a
        // second error -- "Nonexistent function" -- and abort whoever called,
        // which is neither true nor what a GDScript method that errors does.
        error->error = GDEXTENSION_CALL_OK;
        *reinterpret_cast<Variant *>(answer) = Variant();
        return;
    }

    error->error = GDEXTENSION_CALL_OK;

    // 15.5: calling a body that wrote yield^ makes a coroutine and runs
    // nothing. The engine has no use for one, and dropping it here was what
    // made such a member look like it did nothing at all -- so it is driven
    // instead, and the call answers nothing the way the p^ it came from does.
    if (lhat_is_object_kind(ran.value, LHAT_OBJECT_COROUTINE)) {
        int64_t id = 0;
        if (it->script->park_coroutine(ran.value, &id)) {
            drive_await(it, id);
        }
        return;
    }
    *reinterpret_cast<Variant *>(answer) = host::to_variant(ran.value, it->script->godot());
}

// 02 の 8.8: a notification is a number the engine sends, and L^ has no form
// for one. A unit that wants NOTIFICATION_READY writes _ready, which the
// engine calls through instance_call above.
void instance_notification(GDExtensionScriptInstanceDataPtr data, int32_t what,
                           GDExtensionBool reversed)
{
    (void)data;
    (void)what;
    (void)reversed;
}

void instance_to_string(GDExtensionScriptInstanceDataPtr data,
                        GDExtensionBool *is_valid, GDExtensionStringPtr out)
{
    Instance *it = of(data);
    *reinterpret_cast<String *>(out) =
        String("[L^ ") + it->script->get_path() + String("]");
    *is_valid = true;
}

// ---------------------------------------------------------------------------
// What the instance belongs to

void instance_refcount_incremented(GDExtensionScriptInstanceDataPtr data)
{
    (void)data;
}

GDExtensionBool instance_refcount_decremented(
    GDExtensionScriptInstanceDataPtr data)
{
    (void)data;
    return true;  // nothing here holds the owner up
}

GDExtensionObjectPtr instance_owner(GDExtensionScriptInstanceDataPtr data)
{
    return of(data)->owner ? of(data)->owner->_owner : nullptr;
}

GDExtensionObjectPtr instance_script(GDExtensionScriptInstanceDataPtr data)
{
    return of(data)->script.ptr() ? of(data)->script->_owner : nullptr;
}

GDExtensionScriptLanguagePtr instance_language(
    GDExtensionScriptInstanceDataPtr data)
{
    (void)data;
    LhatLanguage *language = LhatLanguage::get_singleton();
    return language != nullptr ? language->_owner : nullptr;
}

GDExtensionBool instance_is_placeholder(GDExtensionScriptInstanceDataPtr data)
{
    (void)data;
    return false;
}

void instance_free(GDExtensionScriptInstanceDataPtr data)
{
    Instance *it = of(data);
    // Only from the table this instance was actually rooted in. The ids start
    // again with each machine, so unrooting after a reload would take out
    // whichever live instance had been given the same number.
    if (it->script.is_valid() &&
        it->script->lhat_generation() == it->born) {
        it->script->drop_instance(it->id);
    }
    if (lhat_is_object(it->self)) {
        instances_by_self().erase(lhat_as_object(it->self));
    }
    memdelete(it);
}

const GDExtensionScriptInstanceInfo3 instance_info = {
    instance_set,
    instance_get,
    instance_property_list,
    instance_free_property_list,
    nullptr,  // get_class_category: the default will do

    instance_property_can_revert,
    instance_property_get_revert,

    instance_owner,
    instance_property_state,

    instance_method_list,
    instance_free_method_list,
    instance_property_type,
    instance_validate_property,

    instance_has_method,

    instance_method_argument_count,

    instance_call,
    instance_notification,

    instance_to_string,

    instance_refcount_incremented,
    instance_refcount_decremented,

    instance_script,

    instance_is_placeholder,

    nullptr,  // set_fallback
    nullptr,  // get_fallback

    instance_language,

    instance_free,
};

}  // namespace

void *lhat_instance_create(LhatScript *script, Object *owner)
{
    if (script == nullptr) {
        return nullptr;
    }
    LhatValue self = lhat_nil();
    int64_t id = 0;
    // 05 の 3.2: an editor script declares no class, so there is no `new` to
    // call and no self^ for it to answer with. What the editor puts this on
    // is its own throwaway EditorScript, and the one thing it asks for is
    // _run -- which instance_call answers off the script rather than off a
    // receiver that does not exist.
    if (!script->is_editor_script() &&
        !script->make_instance(owner, &self, &id)) {
        return nullptr;
    }

    Instance *it = memnew(Instance);
    it->owner = owner;
    it->script = Ref<LhatScript>(script);
    it->self = self;
    it->id = id;
    it->born = script->lhat_generation();
    script->worn_by_object(owner);

    GDExtensionScriptInstanceDataPtr made =
        internal::gdextension_interface_script_instance_create3(&instance_info,
                                                                it);
    if (lhat_is_object(self)) {
        instances_by_self().insert(lhat_as_object(self), it);
    }
    return made;
}

namespace {

// Where a fresh instance could not be made -- a new of the writer's own that
// reaches through the node it was not given -- the type's own zero stands in,
// so the field is at least shown and at least the right shape.
Variant blank_of(LhatUnitTypeKind kind)
{
    switch (kind) {
        case LHAT_UNIT_TYPE_NUMBER: return Variant(0.0);
        case LHAT_UNIT_TYPE_STRING: return Variant(String());
        case LHAT_UNIT_TYPE_BOOL:   return Variant(false);
        default:                    return Variant();
    }
}

}  // namespace

void *lhat_instance_wearing(LhatValue self)
{
    if (!lhat_is_object(self)) {
        return nullptr;
    }
    const void *key = lhat_as_object(self);
    if (GDExtensionScriptInstanceDataPtr *found =
            instances_by_self().getptr(key)) {
        return *found;
    }
    return nullptr;
}

Dictionary lhat_instance_members(void *instance)
{
    Dictionary out;
    Instance *it = (Instance *)instance;
    if (it == nullptr || !living(it) ||
        !lhat_is_object_kind(it->self, LHAT_OBJECT_TABLE)) {
        return out;
    }
    // 14.3 fixes an instance's fields when it is made, so what a fresh one
    // holds is the list of what any of them holds -- and the script already
    // read that off the definition (read_defaults).
    const LhatTable *table = (const LhatTable *)lhat_as_object(it->self);
    LhatMachine *machine = it->script->lhat_machine();
    PackedStringArray names;
    Array values;
    const Array said = it->script->lhat_defaults().keys();
    for (int at = 0; at < said.size(); at++) {
        const String named = said[at];
        CharString spelt = named.utf8();
        LhatValue key = lhat_nil();
        if (!lhat_machine_make_string(machine, spelt.get_data(),
                                      (size_t)spelt.length(), &key)) {
            continue;
        }
        names.push_back(named);
        values.push_back(host::to_variant(lhat_table_get(table, key),
                                          it->script->godot()));
    }
    out["members"] = names;
    out["values"] = values;
    return out;
}

void *lhat_placeholder_create(LhatScript *script, Object *owner)
{
    LhatLanguage *language = LhatLanguage::get_singleton();
    if (script == nullptr || owner == nullptr || language == nullptr) {
        return nullptr;
    }
    void *made =
        internal::gdextension_interface_placeholder_script_instance_create(
            language->_owner, script->_owner, owner->_owner);
    if (made == nullptr) {
        return nullptr;
    }
    script->worn_by_object(owner);
    script->placeholder_made(made);

    const LhatUnit *unit = script->lhat_unit();
    if (unit == nullptr) {
        return made;  // nothing checked, so nothing to say it declares
    }
    CharString klass = script->lhat_class_name().utf8();

    Array properties;
    Dictionary values;
    lhat_exported_properties(script, &properties, &values);
    internal::gdextension_interface_placeholder_script_instance_update(
        made, &properties, &values);
    return made;
}

namespace {

// The header the inspector draws above a script's own fields, and -- less
// obviously -- what tells it which class those fields belong to: doc_name is
// read off this, and every row under it looks its documentation up under
// that name (editor_inspector.cpp's update_tree).
//
// Object::get_property_list does not push one; the instance does. A real
// instance is given it by godot-cpp, but a placeholder's list is handed over
// whole (PlaceHolderScriptInstance::get_property_list), so without this the
// rows would be filed under whatever engine class came before and the
// tooltips would find nothing. GDScript pushes it in each of the three
// lists for the same reason.
//
// Shaped as Script::get_class_category does it: the name is what the header
// reads, and the path is what the editor loads to find the class behind it.
Dictionary class_category(const LhatScript *script)
{
    String path = script->get_path();
    String named = script->get_name();
    Dictionary out;
    out["name"] = named.is_empty() ? path.get_file() : named;
    out["class_name"] = StringName();
    out["type"] = Variant::NIL;
    out["hint"] = PROPERTY_HINT_NONE;
    out["hint_string"] = path;
    out["usage"] = PROPERTY_USAGE_CATEGORY;
    return out;
}

}  // namespace


// What the editor is showing for a class it does not run, brought up to date.
// A placeholder holds the list it was last given and reads the script for
// nothing, so after a reload it would go on showing fields the text no longer
// has -- with the values of whichever ones it kept. Every reload ends here.
void lhat_refresh_placeholders(LhatScript *script)
{
    if (script == nullptr || script->placeholder_list().is_empty()) {
        return;
    }
    Array properties;
    Dictionary values;
    lhat_exported_properties(script, &properties, &values);
    for (void *const &placeholder : script->placeholder_list()) {
        internal::gdextension_interface_placeholder_script_instance_update(
            placeholder, &properties, &values);
    }
}

void lhat_exported_properties(const LhatScript *script, Array *properties,
                              Dictionary *values)
{
    const LhatUnit *unit = script != nullptr ? script->lhat_unit() : nullptr;
    if (unit == nullptr) {
        return;  // nothing checked, so nothing to say it declares
    }
    if (properties != nullptr) {
        properties->push_back(class_category(script));
    }
    CharString klass = script->lhat_class_name().utf8();
    const Dictionary &defaults = script->lhat_defaults();

    size_t count = lhat_unit_member_count(unit, klass.get_data());
    for (size_t i = 0; i < count; i++) {
        LhatUnitMember member = lhat_unit_member(unit, klass.get_data(), i);
        if (member.name == NULL) {
            continue;
        }
        String name = String::utf8(member.name, (int)member.name_length);

        String hint_string;
        uint32_t hint = PROPERTY_HINT_NONE;
        if (!exported_as(script, name, &hint_string, &hint)) {
            continue;
        }

        // What a fresh instance holds says both what the field starts as and
        // what shape it is -- 14.11 ran the initialiser, so the value is the
        // written one. The declaration answers where it holds nil^, and the
        // tree where no instance could be made at all.
        Variant held = defaults.has(name) ? Variant(defaults[name]) : Variant();
        Variant::Type type = Variant::NIL;
        StringName class_name;
        uint32_t usage =
            PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SCRIPT_VARIABLE;
        drawn_as(script, name, member.type, held, &type, &class_name, &hint,
                 &hint_string, &usage);

        Dictionary info;
        info["name"] = name;
        info["class_name"] = class_name;
        info["hint"] = hint;
        info["hint_string"] = hint_string;
        info["usage"] = usage;
        info["type"] = type;
        if (values != nullptr) {
            (*values)[name] = held.get_type() != Variant::NIL
                                  ? held
                                  : UtilityFunctions::type_convert(Variant(),
                                                                   (int)type);
        }
        if (properties != nullptr) {
            properties->push_back(info);
        }
    }
}

}  // namespace godot
