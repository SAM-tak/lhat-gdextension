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

// The engine's classes: one host type per class, each declared under the one
// it inherits from (05 の 8.8改), and the module's tag table filled from
// them. First of everything register_godot does -- godot.Object is the root
// of this tree, and the members registered onto it are registered onto
// something this made.
bool register_godot_classes(LhatProgram *program, Godot *module);

// Their methods, each a member of the class that declares it. After the value
// types, whose names the signatures here spell (godot.Vector2 and the rest).
//
// The first call also finds the MethodBind for each; the module is the
// process's, so the ones after it register the names again and find nothing
// again (8.7). 8.8改 settles what a derived type inherits when registration
// closes, so registering members after the relation is declared is the
// ordinary order rather than a late one.
bool register_godot_api(LhatProgram *program, Godot *module);

// Whether this build has the editor API at all. An editor binary does --
// including the game it launches, which is the same binary -- and an
// exported template does not. What was registered for the editor is skipped
// there rather than declared against a class that is not present, and the
// typed arrays whose element is an editor class go the same way
// (register_containers).
bool has_editor_api();

}  // namespace host
}  // namespace godot

#endif  // LHAT_GODOT_API_GEN_H
