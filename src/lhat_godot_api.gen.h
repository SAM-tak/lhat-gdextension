// L^ (lhat) -- the bound engine methods, registered.
//
// The table itself is generated (lhat_godot_api.gen.cpp, written by
// scripts/gen-godot-api.py); this is the one name the rest of the extension
// reaches it by. What each entry means is in lhat_godot_module.h, beside
// BoundMethod and bound_call.

#ifndef LHAT_GODOT_API_GEN_H
#define LHAT_GODOT_API_GEN_H

#include "lhat.h"

namespace godot {
namespace host {

struct Godot;

// Registers every bound method into `program`, under godot.api.<Class>. The
// first call also finds the MethodBind for each; the module is the process's,
// so the ones after it register the names again and find nothing again
// (05 の 8.7). Belongs with the rest of register_godot, before any checking.
bool register_godot_api(LhatProgram *program, const Godot *module);

}  // namespace host
}  // namespace godot

#endif  // LHAT_GODOT_API_GEN_H
