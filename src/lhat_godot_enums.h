// L^ (lhat) -- the engine's enums, as 02 の 19 章 declares one.
//
// The tables themselves are generated (lhat_godot_api.gen.cpp); this is what
// puts them into a program and, once a machine has them, what finds the
// member an answer's number names.

#ifndef LHAT_GODOT_ENUMS_H
#define LHAT_GODOT_ENUMS_H

#include "lhat.h"

namespace godot {
namespace host {

struct Godot;
struct BoundEnum;
struct BoundConstant;

// The generated tables (lhat_godot_api.gen.cpp). Declared here so that the
// definitions there have external linkage -- a const at namespace scope has
// none of its own -- and so that this file is the one place that names them.
extern BoundEnum enums[];
extern BoundEnum editor_enums[];
extern BoundConstant constants[];
extern BoundConstant editor_constants[];
extern const size_t enum_count;
extern const size_t editor_enum_count;
extern const size_t constant_count;
extern const size_t editor_constant_count;

// 05 の 8.7改2: every enum the generator kept, declared under the type or the
// module that holds it, and every bitfield's names as constants beside them.
// Before the check, as every registration is.
bool register_enums(LhatProgram *program, Godot *module);

// The other half, once lhat_program_install has built them: each enum read
// back off L^.modules and its members put where an answer can find them by
// number. An enumerator is a singleton, so this is the only way a host has
// to hand one over.
void find_enum_members(LhatMachine *machine);

// And the way back: what those left behind, given up when the world is.
void forget_enum_members();

// The member of `held` whose number is `value`, or nil^ where the engine
// answered something the enum does not name -- which a bitfield packed into
// an enum-typed answer can do.
LhatValue enum_member(const BoundEnum *held, int64_t value);

}  // namespace host
}  // namespace godot

#endif  // LHAT_GODOT_ENUMS_H
