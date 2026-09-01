#include "lhat_godot_enums.h"

#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/local_vector.hpp>

#include "lhat_godot_api.gen.h"
#include "lhat_godot_module.h"

namespace godot {
namespace host {

namespace {

// What `BoundEnum::by_value` points at. Kept off the row so that the table
// stays what the generator wrote and this stays what a machine gave.
using ByValue = HashMap<int64_t, LhatValue>;

// Every map made, so that giving them up is one walk rather than a search
// through two tables.
LocalVector<ByValue *> &made()
{
    static LocalVector<ByValue *> it;
    return it;
}

bool declare(LhatProgram *program, BoundEnum *held, size_t how_many)
{
    for (size_t at = 0; at < how_many; at++) {
        const BoundEnum &one = held[at];
        if (!lhat_register_enum_valued(program, one.module, one.type, one.name,
                                       one.members, one.values, one.count)) {
            return false;
        }
    }
    return true;
}

bool declare(LhatProgram *program, const BoundConstant *held, size_t how_many)
{
    for (size_t at = 0; at < how_many; at++) {
        const BoundConstant &one = held[at];
        if (!lhat_register_const_integer(program, one.module, one.type,
                                         one.name, one.value)) {
            return false;
        }
    }
    return true;
}

// 19.2: the enum's own table is name -> enumerator, sealed at birth. What an
// answer has is the number instead, so the walk is turned round once here
// rather than searched on every call.
void read_back(LhatMachine *machine, BoundEnum *held, size_t how_many)
{
    for (size_t at = 0; at < how_many; at++) {
        BoundEnum &one = held[at];
        LhatValue stands = lhat_nil();
        if (!lhat_machine_registered(machine, one.module, one.type, one.name,
                                     &stands) ||
            !lhat_is_object_kind(stands, LHAT_OBJECT_ENUM)) {
            continue;
        }
        const LhatEnum *declared = (const LhatEnum *)lhat_as_object(stands);
        if (declared->members == nullptr) {
            continue;
        }
        ByValue *by_value = memnew(ByValue);
        for (size_t i = 0; i < one.count; i++) {
            LhatValue member = lhat_table_get_bytes(declared->members,
                                                    one.members[i],
                                                    strlen(one.members[i]));
            if (lhat_is_object_kind(member, LHAT_OBJECT_ENUMERATOR)) {
                // Two names of one number is a thing Godot's enums do
                // (ERR_* aliases); the first is the one an answer comes back
                // as, which is the order the engine wrote them in.
                if (!by_value->has(one.values[i])) {
                    by_value->insert(one.values[i], member);
                }
            }
        }
        one.by_value = by_value;
        made().push_back(by_value);
    }
}

}  // namespace

bool register_enums(LhatProgram *program, Godot *module)
{
    (void)module;
    if (program == nullptr) {
        return false;
    }
    if (!declare(program, enums, enum_count) ||
        !declare(program, constants, constant_count)) {
        return false;
    }
    // 8.8改: what the editor API declares leaves with the editor classes --
    // an exported game has neither the types these hang under nor the
    // methods that name them.
    if (!has_editor_api()) {
        return true;
    }
    return declare(program, editor_enums, editor_enum_count) &&
           declare(program, editor_constants, editor_constant_count);
}

void find_enum_members(LhatMachine *machine)
{
    if (machine == nullptr) {
        return;
    }
    forget_enum_members();
    read_back(machine, enums, enum_count);
    if (has_editor_api()) {
        read_back(machine, editor_enums, editor_enum_count);
    }
}

void forget_enum_members()
{
    for (ByValue *const &one : made()) {
        memdelete(one);
    }
    made().clear();
    for (size_t at = 0; at < enum_count; at++) {
        enums[at].by_value = nullptr;
    }
    for (size_t at = 0; at < editor_enum_count; at++) {
        editor_enums[at].by_value = nullptr;
    }
}

LhatValue enum_member(const BoundEnum *held, int64_t value)
{
    if (held == nullptr || held->by_value == nullptr) {
        return lhat_nil();
    }
    const ByValue *by_value = (const ByValue *)held->by_value;
    const LhatValue *found = by_value->getptr(value);
    return found != nullptr ? *found : lhat_nil();
}

}  // namespace host
}  // namespace godot
