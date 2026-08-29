// L^ (lhat) -- the engine's global functions, as functions of the godot module.
//
// These are not ClassDB methods and there is no receiver to write before
// them: Godot calls them utility functions and GDScript writes them bare.
// So they land the way a singleton's methods land (05 の 8.7) -- functions of
// a module -- and the module is `godot` itself, which makes godot.randf()
// read as GDScript's randf() does.
//
// Written out rather than generated. There are 114 of them and only these
// earn a place: 78 are mathematics that std.math already answers, and the
// nine that take a variadic tail have no ptrcall for the generator to find.
// godot-cpp wraps every one as a static of its own, so what is left here is
// the crossing and nothing else.

#ifndef LHAT_GODOT_UTIL_H
#define LHAT_GODOT_UTIL_H

#include "lhat.h"

namespace godot {
namespace host {

struct Godot;

// Registers them all. Before any checking (05 の 8.7).
bool register_util(LhatProgram *program, Godot *module);

}  // namespace host
}  // namespace godot

#endif  // LHAT_GODOT_UTIL_H
