#include "lhat_debugger.h"

#include <godot_cpp/classes/engine_debugger.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string_name.hpp>

#include <cstring>

#include "lhat/debug.h"
#include "lhat_godot_module.h"
#include "lhat_instance.h"
#include "lhat_godot_values.h"
#include "lhat_variant.h"

namespace godot {
namespace host {
namespace {

// Held behind a call rather than written as a global: a godot-cpp object is
// built through the interface, and a global's constructor runs when the
// library is loaded, which is before the interface is there.
struct Watch {
    // The one machine a world has (05 の 5.3), remembered where it is made
    // so that the virtuals can read its frames without reaching back into
    // the language.
    LhatMachine *machine = nullptr;
    ScriptLanguage *language = nullptr;
    bool stopped = false;
    // 09 の 2.3 says the hook is told about lines and nothing else, so the
    // depth a step wants is taken from how the frame count moved since the
    // last line rather than from entering and leaving. The arithmetic below
    // is GDScript's (gdscript_vm.cpp's OPCODE_LINE and gdscript.h's
    // enter_function), and the two agree because entering is what raises the
    // count and leaving is what lowers it.
    int last_frames = -1;
};

Watch &watching()
{
    static Watch it;
    return it;
}

// units_for holds a unit under the path with its scheme cut off, and the
// editor names a breakpoint by the path it shows -- res://spinner.lh.
String openable(const char *source)
{
    if (source == nullptr) {
        return String();
    }
    String path = String::utf8(source);
    if (path.begins_with("res://") || path.begins_with("user://")) {
        return path;
    }
    return String("res://") + path;
}

// 09 の 2.3: the machine's own thread, between two instructions, and it waits
// for this to return. So this is where a program is stopped.
void reached_a_line(LhatMachine *machine, void *context, LhatDebugEvent event,
                    const LhatFrameInfo *where)
{
    (void)context;
    if (event != LHAT_DEBUG_LINE || where == nullptr) {
        return;
    }
    EngineDebugger *debugger = EngineDebugger::get_singleton();
    if (debugger == nullptr || !debugger->is_active()) {
        return;
    }
    Watch &watch = watching();

    const int frames = (int)lhat_machine_fault_depth(machine);
    const int64_t left = debugger->get_lines_left();
    const int64_t deep = debugger->get_depth();
    if (watch.last_frames >= 0 && left > 0 && deep >= 0) {
        debugger->set_depth(deep + (frames - watch.last_frames));
    }
    watch.last_frames = frames;

    bool stop = false;
    // A step: run this many more lines and stop. Only lines at or above the
    // level the step began at count, which is what keeps `next` from
    // stepping into a call -- and what `stepIn` turns off by asking for a
    // depth below zero.
    if (debugger->get_lines_left() > 0) {
        if (debugger->get_depth() <= 0) {
            debugger->set_lines_left(debugger->get_lines_left() - 1);
        }
        if (debugger->get_lines_left() <= 0) {
            stop = true;
        }
    }
    if (debugger->is_breakpoint(where->line,
                                StringName(openable(where->source))) &&
        !debugger->is_skipping_breakpoints()) {
        stop = true;
    }

    if (stop && watch.language != nullptr) {
        // Blocks: the editor's loop runs from here and asks the virtuals in
        // lhat_language.cpp, which read the frames standing right now.
        watch.stopped = true;
        debugger->script_debug(watch.language, true, false);
        watch.stopped = false;
        // Whatever the editor asked for, the step begins again from here.
        watch.last_frames = (int)lhat_machine_fault_depth(machine);
    }
    debugger->line_poll();
}

// 14.4 keeps self^ out of the parameters and 09 の 3.2 counts it among the
// names the language binds, so this is where a frame says whose method it is.
LhatValue frame_self(LhatMachine *machine, size_t level)
{
    const size_t locals = lhat_frame_local_count(machine, level);
    for (size_t i = 0; i < locals; i++) {
        LhatBindingInfo said = {};
        if (lhat_frame_local(machine, level, i, &said) &&
            said.name != nullptr && strcmp(said.name, "self^") == 0) {
            return said.value;
        }
    }
    return lhat_nil();
}

// 09 の 3.2: a host value arrives in its pointer form aimed into the stack,
// which to_variant does not read -- it reads a box, because an any^ is where
// it looks and 8.9 keeps a bare value out of one.
Variant shown(LhatValue value, const Godot *module)
{
    bool valued = false;
    Variant held = variant_of_value(value, module, &valued);
    return valued ? held : to_variant(value, module);
}

}  // namespace

void watch(LhatMachine *machine, ScriptLanguage *language)
{
    if (machine == nullptr) {
        return;
    }
    EngineDebugger *debugger = EngineDebugger::get_singleton();
    const bool wanted = debugger != nullptr && debugger->is_active();
    Watch &watch = watching();
    watch.machine = machine;
    watch.language = language;
    watch.stopped = false;
    watch.last_frames = -1;
    lhat_machine_set_debug_hook(machine, wanted ? reached_a_line : nullptr,
                                nullptr);
}

bool stopped()
{
    return watching().stopped;
}

int32_t frame_count()
{
    if (!stopped()) {
        return (int32_t)last_fault_frames().size();
    }
    LhatMachine *machine = watching().machine;
    return machine != nullptr ? (int32_t)lhat_machine_fault_depth(machine) : 0;
}

bool frame_at(int32_t level, FaultFrame *out)
{
    if (level < 0) {
        return false;
    }
    if (!stopped()) {
        const Vector<FaultFrame> &frames = last_fault_frames();
        if (level >= frames.size()) {
            return false;
        }
        *out = frames[level];
        return true;
    }
    LhatMachine *machine = watching().machine;
    LhatFrameInfo said = {};
    if (machine == nullptr ||
        !lhat_machine_fault_frame(machine, (size_t)level, &said)) {
        return false;
    }
    out->source = openable(said.source);
    out->name = said.name != nullptr ? String::utf8(said.name)
                : said.top_level     ? String("<top level>")
                : said.coroutine     ? String("<walk>")
                : said.disposing     ? String("<cleanup>")
                                     : String("<anonymous>");
    out->line = (int32_t)said.line;
    return true;
}

void *frame_instance(int32_t level)
{
    LhatMachine *machine = watching().machine;
    if (!stopped() || machine == nullptr || level < 0) {
        return nullptr;
    }
    return lhat_instance_wearing(frame_self(machine, (size_t)level));
}

Dictionary frame_members(int32_t level)
{
    return lhat_instance_members(frame_instance(level));
}

Dictionary frame_locals(int32_t level)
{
    Dictionary out;
    LhatMachine *machine = watching().machine;
    if (!stopped() || machine == nullptr || level < 0) {
        return out;
    }
    const Godot *module = shared_godot();
    PackedStringArray names;
    Array values;
    const size_t at = (size_t)level;
    // 09 の 3.2: the locals live at this frame's instruction, then the
    // captures. Both are what the writer can see from where the program
    // stopped, which is what the panel is for.
    const size_t locals = lhat_frame_local_count(machine, at);
    for (size_t i = 0; i < locals; i++) {
        LhatBindingInfo said = {};
        if (!lhat_frame_local(machine, at, i, &said)) {
            continue;
        }
        names.push_back(said.name != nullptr ? String::utf8(said.name)
                                             : String("?"));
        values.push_back(shown(said.value, module));
    }
    const size_t held = lhat_frame_upvalue_count(machine, at);
    for (size_t i = 0; i < held; i++) {
        LhatBindingInfo said = {};
        if (!lhat_frame_upvalue(machine, at, i, &said)) {
            continue;
        }
        names.push_back(said.name != nullptr ? String::utf8(said.name)
                                             : String("?"));
        values.push_back(shown(said.value, module));
    }
    out["locals"] = names;
    out["values"] = values;
    return out;
}

}  // namespace host
}  // namespace godot
