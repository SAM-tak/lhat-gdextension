// L^ (lhat) -- the engine's debugger, driven off 09 章's hook.
//
// The machine calls back when it reaches a new line, and everything a
// debugger does is built on that: a breakpoint is a line the editor named,
// stepping is a count of lines to run before stopping, and stopping is
// EngineDebugger::script_debug, which blocks here while the editor's own
// loop asks what the frames hold.
//
// None of it is Godot-specific on the far side. The editor's DAP server
// (editor/debugger/debug_adapter/) names no language: VSCode's godot-tools
// speaks DAP to the editor, the editor speaks the remote debugger protocol
// to the running game, and the game asks ScriptLanguage. So filling these in
// is what puts L^ in the editor's debugger panel AND in VSCode, with no
// adapter of our own.
//
// What Godot does not ask for, and so is not here: writing a binding back
// (there is no setVariable in its DAP and no setter on ScriptLanguage), and
// evaluating an L^ expression (its `evaluate` collects our locals and runs
// them through Godot's own Expression instead).

#ifndef LHAT_DEBUGGER_H
#define LHAT_DEBUGGER_H

#include <godot_cpp/classes/script_language.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include "lhat.h"
#include "lhat_host.h"

namespace godot {
namespace host {

// Puts the hook on the machine when a debugger is attached, and takes it off
// otherwise -- 09 の 2.2: set, every instruction pays one test; unset, one
// branch not taken. Called wherever a machine is made.
void watch(LhatMachine *machine, ScriptLanguage *language);

// True while the machine is stopped inside the hook. What the panel is shown
// comes from the live frames then, and from the fault run_problem recorded
// otherwise.
bool stopped();

// How many frames the panel should see, and one of them. Reads the live
// frames while stopped and host::last_fault_frames() when not.
int32_t frame_count();
bool frame_at(int32_t level, FaultFrame *out);

// {"locals": PackedStringArray, "values": Array}, which is the shape
// ScriptLanguageExtension unpacks (script_language_extension.h). Empty when
// nothing is stopped -- a fault's registers are gone by the time the panel
// asks, and only a live frame has any.
Dictionary frame_locals(int32_t level);

// The frame's self^, paired back to the script instance the engine knows
// (lhat_instance.h), and that instance's own fields. NULL and empty for a
// frame that is not a method's -- a bare f^, the unit's top level.
void *frame_instance(int32_t level);
Dictionary frame_members(int32_t level);

// 09 の 3.5: `text` compiled as one input with the frame's names in scope
// and run on the machine, answered as the text a panel shows. The one place
// an L^ expression is written to a debugger: the engine's own `evaluate`
// collects our locals and runs them through Godot's Expression instead, so
// it reads GDScript and this reads L^.
String frame_evaluate(int32_t level, const String &text);

}  // namespace host
}  // namespace godot

#endif  // LHAT_DEBUGGER_H
