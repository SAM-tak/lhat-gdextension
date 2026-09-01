#include "lhat_language.h"

#include <godot_cpp/templates/local_vector.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <string.h>

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_settings.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/script_language_extension_profiling_info.hpp>
#include <godot_cpp/core/memory.hpp>

#include "lhat.h"
#include "lhat_godot_enums.h"
#include "lhat_godot_module.h"
#include "lhat_debugger.h"
#include "lhat_host.h"
#include "lhat_script.h"

namespace godot {

LhatLanguage *LhatLanguage::singleton = nullptr;

namespace {

// The groups vscode-extension/syntaxes/lhat.tmLanguage.json already sorts
// these into, kept alike so the two editors read alike. Written with the hat,
// since that is the spelling: what strips it is unhatted() below, and only
// for the one caller that needs it stripped.
const char *const control_flow_words[] = {
    "if^",     "else^",     "elseif^", "elsif^",  "elif^",   "ei^",
    "el^",     "for^",      "when^",   "other^",  "from^",   "to^",
    "downto^", "step^",     "in^",     "while^",  "until^",  "next^",
    "skip^",   "continue^", "repeat^", "do^",     "break^",  "return^",
    "yield^",  "await^",    "defer^",  "with^",   "finally^",
    "try^",    "catch^",    "panic^",  nullptr,
};

// What declares, and the operators 01 の 7 章 spells as words. true^ and
// false^ are here rather than among the values below: they are constants, and
// a theme telling the two apart draws a constant with its keywords.
//
// type^ appears in no design document. Left where it has always been rather
// than moved on a guess about what it would be.
const char *const declaring_words[] = {
    "var^",      "let^",     "f^",       "p^",        "op^",
    "def^",      "enum^",    "pack^",    "unpack^",   "module^",
    "public^",   "require^", "import^",  "override^", "overload^",
    "abstract^", "closed^",  "errordef^", "delegate^",
    "true^",     "false^",
    "and^",      "or^",      "is^",      "fits^",     "as^", nullptr,
};

// Self^ is a type where self^ is a value: 01 の 2.3 compares a hat word byte
// for byte, so the capital is a different name and belongs to another list.
const char *const type_words[] = {
    "number^", "string^", "bool^",  "nil^",  "any^",
    "t^",      "c^",      "error^", "Self^", nullptr,
};

// The hat words that name a value rather than syntax. 05 の 8.6's L^ names
// the machine; the rest name what the definition around them is applied to.
const char *const value_words[] = {
    "this^", "it^", "self^", "super^", "def^", "L^", nullptr,
};

// 02 の 9.2's clause words, each with the long spelling the parser also takes.
const char *const clause_words[] = {
    "typeof^",   "id^",      "width^",   "autowidth^", "prolog^",
    "prologue^", "pre^",     "premain^", "first^",     "main^",
    "last^",     "epilog^",  "epilogue^", nullptr,
};

bool word_among(const char *const *list, const char *text, size_t length)
{
    for (size_t i = 0; list[i] != nullptr; i++) {
        if (strlen(list[i]) == length && memcmp(list[i], text, length) == 0) {
            return true;
        }
    }
    return false;
}

// The word the editor's own CodeHighlighter can match: the spelling above
// without its hat. That highlighter ends a word at the first character an
// identifier is not made of, and '^' is not one -- so a word registered with
// the hat is compared against text that can never contain it and never
// matches. Measured: 'def^' registered against the line 'def^ x' colours
// nothing, 'def' colours the three characters before the hat.
//
// 01 の 2.3 makes the hat part of the name, so this is not the name; it is
// the run of characters that highlighter will have in hand. What it costs is
// a bare 'def' -- a different name, and a legal one -- coloured as the
// keyword. Only the fallback pays it: LhatHighlighter reads whole words and
// does not go through here.
String unhatted(const char *word)
{
    String spelt = String::utf8(word);
    return spelt.ends_with("^") ? spelt.substr(0, spelt.length() - 1) : spelt;
}

}  // namespace

const char *const *lhat_words_of(LhatWordKind kind)
{
    switch (kind) {
        case LHAT_WORD_CONTROL: return control_flow_words;
        case LHAT_WORD_DECLARE: return declaring_words;
        case LHAT_WORD_TYPE:    return type_words;
        case LHAT_WORD_VALUE:   return value_words;
        case LHAT_WORD_CLAUSE:  return clause_words;
        default:                return nullptr;
    }
}

LhatWordKind lhat_word_kind(const char *text, size_t length)
{
    // 01 の 2.3: the hat is part of the name, and a spelling with more than
    // one is that same name reached further out -- so the word to compare is
    // the letters plus a single hat.
    size_t hats = 0;
    while (hats < length && text[length - 1 - hats] == '^') {
        hats++;
    }
    if (hats == 0) {
        return LHAT_WORD_NONE;  // no hat, so none of these
    }
    size_t canonical = length - hats + 1;

    static const LhatWordKind kinds[] = {LHAT_WORD_CONTROL, LHAT_WORD_DECLARE,
                                         LHAT_WORD_TYPE, LHAT_WORD_VALUE,
                                         LHAT_WORD_CLAUSE};
    for (LhatWordKind kind : kinds) {
        if (word_among(lhat_words_of(kind), text, canonical)) {
            return kind;
        }
    }
    return LHAT_WORD_NONE;
}

LhatLanguage::LhatLanguage()
{
    singleton = this;
}

LhatLanguage::~LhatLanguage()
{
    if (singleton == this) {
        singleton = nullptr;
    }
}

LhatLanguage *LhatLanguage::get_singleton()
{
    return singleton;
}

void LhatLanguage::_bind_methods()
{
}

String LhatLanguage::_get_name() const
{
    return "L^";
}

String LhatLanguage::_get_type() const
{
    return "LhatScript";
}

String LhatLanguage::_get_extension() const
{
    return "lh";
}

PackedStringArray LhatLanguage::_get_recognized_extensions() const
{
    PackedStringArray out;
    out.push_back("lh");
    return out;
}

// 05 の 5.3: one program for the process, made on the first ask. Every .lh is
// a unit of it, so a require^ from one script reaches the very unit another
// script's require^ reached -- one table, one definition, one set of types.
LhatProgram *LhatLanguage::world_program()
{
    if (program != nullptr) {
        return program;
    }
    units = host::units_for("res://");
    program = host::program_for(&units);
    if (program == nullptr) {
        UtilityFunctions::push_error(
            String("L^: out of memory making the program"));
        return nullptr;
    }
    machine = lhat_machine_new();
    if (machine == nullptr) {
        lhat_program_free(program);
        program = nullptr;
        UtilityFunctions::push_error(
            String("L^: out of memory making the machine"));
        return nullptr;
    }
    // 05 の 8.7: what was registered reaches the machine here, which is what
    // makes the names bound above answer something.
    lhat_program_install(program, machine);
    // 05 の 8.7改2: install is what builds the enums, and an answer of one
    // has to be handed the very member the machine made (19.2's singleton).
    // So they are read back here, once, while the machine is new.
    host::find_enum_members(machine);
    // 09 の 2.2: and the hook goes on where a debugger is attached, which is
    // what lets a breakpoint stop this machine (lhat_debugger.h).
    host::watch(machine, this);
    return program;
}

// 05 の 5.7, whole, through lhat_reload. A save used to retire every unit
// and read the whole project back -- invalidate counted what it retired and
// did not name it, so there was nothing to be selective with. lhat_reload
// still only counts, but a retired unit's shell comes back with a new proto,
// and a script remembers the one it ran: that is enough to know which
// scripts to dress again and which to leave alone.
//
// The order matters more than it looks. lhat_reload forgets the retired
// modules and collects each machine before it returns, and a wearer's
// fields are read through its instance's self^, which that collection may
// free. So every wearer's fields are read first, while every instance is
// still there to read -- reading is cheap, it is the dressing that is not --
// and only the scripts the reload reached are dressed again.
//
// A path whose text reads as it did answers 0 from lhat_reload's own
// invalidate and costs nothing further, so offering every unit on a rescan
// is a comparison per unit and a reload only for the ones that moved.
Error LhatLanguage::rebuild_world(const LocalVector<LhatScript *> &changed,
                                  bool everything, bool keep_state)
{
    if (rebuilding) {
        return OK;  // put_back writes scripts, and a write can ask again
    }
    if (world_program() == nullptr) {
        return ERR_OUT_OF_MEMORY;
    }
    rebuilding = true;

    // Held first, since put_back writes to the set the loop below reads.
    LocalVector<Ref<Script>> held;
    for (LhatScript *const &script : LhatScript::all()) {
        held.push_back(Ref<Script>(script));
    }
    LocalVector<LocalVector<Dictionary>> fields;
    fields.resize(held.size());
    for (uint32_t i = 0; i < held.size(); i++) {
        Object::cast_to<LhatScript>(held[i].ptr())->snapshot(&fields[i]);
    }

    // The text as it stands, not the file: the editor reloads while the
    // buffer is unsaved. Held under the path before the program is asked to
    // read it back.
    LocalVector<CharString> paths;
    for (LhatScript *const &script : changed) {
        host::hold(&units, script->get_path(), script->source);
        paths.push_back(host::unit_path(units, script->get_path()).utf8());
    }
    if (everything) {
        for (const LhatUnit *unit = lhat_program_units(program);
             unit != nullptr; unit = lhat_unit_next(unit)) {
            paths.push_back(CharString(lhat_unit_path(unit)));
        }
    }
    LhatMachine *const machines[] = {machine};
    for (const CharString &path : paths) {
        lhat_reload(program, path.get_data(), machines, 1);
    }

    // What the reload reached, run again and dressed again. What it did not
    // reach keeps its instances, and its wearers never notice.
    Error said = OK;
    for (uint32_t i = 0; i < held.size(); i++) {
        LhatScript *script = Object::cast_to<LhatScript>(held[i].ptr());
        if (!script->lhat_stale()) {
            continue;
        }
        LocalVector<uint64_t> wearers;
        for (const uint64_t &id : script->worn_by) {
            wearers.push_back(id);
        }
        script->worn_by.clear();
        Error one = script->reload_now(keep_state);
        if (one != OK) {
            said = one;
        }
        script->put_back(wearers, fields[i]);
    }

    rebuilding = false;
    return said;
}

void LhatLanguage::_init()
{
}

// 05 の 8.7: the identities a registration declares live for the process, and
// so does the module that holds the strings every program borrowed. Both go
// back here, in this order -- the registry is what every program's
// registrations point at, so it is last, and it may only go when no program
// is left. Nothing holds one by now: a script frees its own with the machine
// it made (let_go), and the editor is on its way out.
void LhatLanguage::_finish()
{
    if (machine != nullptr) {
        lhat_machine_dispose(machine);
        machine = nullptr;
    }
    if (program != nullptr) {
        lhat_program_free(program);
        program = nullptr;
    }
    host::dispose_godot();
    lhat_registry_dispose();
}

Dictionary LhatLanguage::_validate(const String &script, const String &path,
                                   bool validate_functions,
                                   bool validate_errors,
                                   bool validate_warnings,
                                   bool validate_safe_lines) const
{
    (void)validate_warnings;
    (void)validate_safe_lines;

    Dictionary out;
    // The members overview beside the code. Read off the text, so it is the
    // buffer's lines that are listed -- and answered whether or not what
    // follows finds the unit sound, since ScriptTextEditor::get_functions
    // takes the list only when the whole answer says valid anyway.
    if (validate_functions) {
        out["functions"] = lhat_subroutine_lines(script);
    }

    // The text, not the file -- the editor asks while the buffer is unsaved,
    // and for a script whose file does not exist yet.
    host::Units units = host::units_for(path);
    host::hold(&units, path, script);

    LhatProgram *program = host::program_for(&units);
    if (program == nullptr) {
        out["valid"] = false;
        return out;
    }

    const LhatUnit *root =
        lhat_program_check(program, units.path.utf8().get_data());
    bool ok = root != nullptr && !lhat_program_has_errors(program);
    out["valid"] = ok;
    if (!ok && validate_errors) {
        out["errors"] = host::diagnostics_as_errors(program, path);
    }
    lhat_program_free(program);
    return out;
}

// Empty means the path will do. 5.1 takes a path as written, so there is
// nothing here to refuse.
String LhatLanguage::_validate_path(const String &path) const
{
    (void)path;
    return String();
}

Object *LhatLanguage::_create_script() const
{
    return memnew(LhatScript);
}

namespace {

// What the Create Script dialog offers. Godot asks per class and walks the
// chosen base's inheritance chain (script_create_dialog.cpp's
// _update_template_menu), so a row kept under Object is offered for every
// base and a row kept under Node for every node.
//
// Four of these are L^'s own -- a .lh is one of two things (05 の 3.2), and
// the one with a module^ is written three ways: worn by a node, worn by a
// resource, or reached by require^. The rest are GDScript's ten, ported.
struct BuiltInTemplate {
    const char *inherit;
    const char *name;
    const char *description;
    const char *content;
};

// 05 の 8.8改 puts the engine's tree on the host side, so godot._BASE_ is a
// type the checker knows and there is no library to inherit from. What a
// class needs of the engine is the handle it holds, the constructor the
// engine calls, and delegate^ (02 の 14.7改2) to show that class's members
// as its own -- three lines the dialog writes once rather than a unit the
// project carries.
//
// Which engine class it is, is not among them. The extension reads that off
// what new was written to take (LhatScript::read_base_class), so the type
// written there is the only place it is said and there is no second spelling
// for it to disagree with.
const char *const node_template =
    "module^_CLASS_SNAKE_CASE_\n"
    "\n"
    "import^godot\n"
    "\n"
    "@game\n"
    "public^let^_CLASS_ = def^{\n"
    "_TS_self^{\n"
    "_TS__TS_# The node this is worn by. new takes one, and the type\n"
    "_TS__TS_# written there is what says which engine class this may be\n"
    "_TS__TS_# put on; delegate^ below shows that class's members here.\n"
    "_TS__TS_abstract^gdobj : godot._BASE_,\n"
    "_TS__TS_# Add your own properties.\n"
    "_TS__TS_# @export speed = 1, \n"
    "_TS_},\n"
    "\n"
    "_TS_override^new = f^obj { self^{ gdobj = obj } },\n"
    "\n"
    "_TS_delegate^self^.gdobj,\n"
    "\n"
    "_TS_# Once, when the node enters the tree.\n"
    "_TS__ready = p^self^{\n"
    "_TS_},\n"
    "\n"
    "_TS_# Every frame. `delta` is how long the last one took, in seconds.\n"
    "_TS__process = p^self^, delta:number^{\n"
    "_TS_},\n"
    "}\n";

// The same shape for the other thing a script is worn by (05 の 8.8改 again,
// and demo/item.lh): a resource is data the inspector edits and a .tres
// keeps, so what it has instead of _ready is @export fields.
const char *const resource_template =
    "module^_CLASS_SNAKE_CASE_\n"
    "\n"
    "import^godot\n"
    "\n"
    "@export_class\n"
    "public^let^_CLASS_ = def^{\n"
    "_TS_self^{\n"
    "_TS__TS_abstract^gdobj : godot._BASE_,\n"
    "\n"
    "_TS__TS_# What the inspector edits and a .tres keeps.\n"
    "_TS__TS_@export name = \"\",\n"
    "_TS_},\n"
    "\n"
    "_TS_override^new = f^obj { self^{ gdobj = obj } },\n"
    "\n"
    "_TS_delegate^self^.gdobj,\n"
    "}\n";

const char *const module_template =
    "module^_CLASS_SNAKE_CASE_\n"
    "\n"
    "# Nothing wears this one: require^ is what reaches it, and only the\n"
    "# public^ names are answered (05 の 5.5).\n"
    "\n"
    "public^let^greet = f^name:string^ {\n"
    "_TS_return^$\"hello, {name}\"\n"
    "}\n";

const char *const editor_script_template =
    "# No module^, so this is an editor script (05 の 3.2): the body runs on\n"
    "# File > Run and nothing runs it when the project is read. The class the\n"
    "# dialog was opened with is not used -- nothing wears this.\n"
    "\n"
    "print(\"hello from L^\")\n";


// The rest are GDScript's own built-in templates, one for one
// (modules/gdscript/editor/script_templates), written the way L^ writes
// them. Where GDScript assigns velocity.x, L^ makes a new Vector2 -- 8.9's
// values are read by field and replaced whole -- and where GDScript leaves
// an argument to its default, L^ writes it (13.4).
const char *const character_body_2d_template =
    "module^_CLASS_SNAKE_CASE_\n"
    "\n"
    "import^godot\n"
    "\n"
    "# Classic movement for gravity games (platformer, ...).\n"
    "@game\n"
    "public^let^_CLASS_ = def^{\n"
    "_TS_self^{\n"
    "_TS__TS_abstract^gdobj : godot._BASE_,\n"
    "_TS__TS_# GDScript keeps these as constants; here they are fields the\n"
    "_TS__TS_# inspector can edit, which a movement script is the better for.\n"
    "_TS__TS_@export speed = 300.0,\n"
    "_TS__TS_@export jumpVelocity = -400.0,\n"
    "_TS_},\n"
    "_TS_override^new = f^obj { self^{ gdobj = obj } },\n"
    "_TS_delegate^self^.gdobj,\n"
    "\n"
    "_TS__physics_process = p^self^, delta:number^{\n"
    "_TS__TS_var^velocity = self^.get_velocity()\n"
    "\n"
    "_TS__TS_# Add the gravity.\n"
    "_TS__TS_if^!self^.is_on_floor() {\n"
    "_TS__TS__TS_velocity := velocity + self^.get_gravity() * delta\n"
    "_TS__TS_}\n"
    "\n"
    "_TS__TS_# Handle jump.\n"
    "_TS__TS_if^godot.Input.is_action_just_pressed(\"ui_accept\", false^) and^self^.is_on_floor() {\n"
    "_TS__TS__TS_velocity := godot.vector2(velocity.x, self^.jumpVelocity)\n"
    "_TS__TS_}\n"
    "\n"
    "_TS__TS_# Get the input direction and handle the movement/deceleration.\n"
    "_TS__TS_# As good practice, you should replace UI actions with custom gameplay actions.\n"
    "_TS__TS_let^direction = godot.Input.get_axis(\"ui_left\", \"ui_right\")\n"
    "_TS__TS_velocity := godot.vector2(\n"
    "_TS__TS__TS_if^direction != 0.0: direction * self^.speed\n"
    "_TS__TS__TS_el^: godot.move_toward(velocity.x, 0.0, self^.speed);,\n"
    "_TS__TS__TS_velocity.y)\n"
    "\n"
    "_TS__TS_self^.set_velocity(velocity)\n"
    "_TS__TS_self^.move_and_slide()\n"
    "_TS_},\n"
    "}\n";

const char *const character_body_3d_template =
    "module^_CLASS_SNAKE_CASE_\n"
    "\n"
    "import^godot\n"
    "\n"
    "# Classic movement for gravity games (FPS, TPS, ...).\n"
    "@game\n"
    "public^let^_CLASS_ = def^{\n"
    "_TS_self^{\n"
    "_TS__TS_abstract^gdobj : godot._BASE_,\n"
    "_TS__TS_# GDScript keeps these as constants; here they are fields the\n"
    "_TS__TS_# inspector can edit, which a movement script is the better for.\n"
    "_TS__TS_@export speed = 5.0,\n"
    "_TS__TS_@export jumpVelocity = 4.5,\n"
    "_TS_},\n"
    "_TS_override^new = f^obj { self^{ gdobj = obj } },\n"
    "_TS_delegate^self^.gdobj,\n"
    "\n"
    "_TS__physics_process = p^self^, delta:number^{\n"
    "_TS__TS_var^velocity = self^.get_velocity()\n"
    "\n"
    "_TS__TS_# Add the gravity.\n"
    "_TS__TS_if^!self^.is_on_floor() {\n"
    "_TS__TS__TS_velocity := velocity + self^.get_gravity() * delta\n"
    "_TS__TS_}\n"
    "\n"
    "_TS__TS_# Handle jump.\n"
    "_TS__TS_if^godot.Input.is_action_just_pressed(\"ui_accept\", false^) and^self^.is_on_floor() {\n"
    "_TS__TS__TS_velocity := godot.vector3(velocity.x, self^.jumpVelocity, velocity.z)\n"
    "_TS__TS_}\n"
    "\n"
    "_TS__TS_# Get the input direction and handle the movement/deceleration.\n"
    "_TS__TS_# As good practice, you should replace UI actions with custom gameplay actions.\n"
    "_TS__TS_let^input = godot.Input.get_vector(\"ui_left\", \"ui_right\", \"ui_up\", \"ui_down\", -1.0)\n"
    "_TS__TS_let^direction = (self^.get_basis() * godot.vector3(input.x, 0.0, input.y)).normalized()\n"
    "_TS__TS_velocity := if^direction.length() > 0.0:\n"
    "_TS__TS__TS_godot.vector3(direction.x * self^.speed, velocity.y, direction.z * self^.speed)\n"
    "_TS__TS_el^:\n"
    "_TS__TS__TS_godot.vector3(godot.move_toward(velocity.x, 0.0, self^.speed), velocity.y,\n"
    "_TS__TS__TS__TS_godot.move_toward(velocity.z, 0.0, self^.speed));\n"
    "\n"
    "_TS__TS_self^.set_velocity(velocity)\n"
    "_TS__TS_self^.move_and_slide()\n"
    "_TS_},\n"
    "}\n";

const char *const editor_plugin_template =
    "module^_CLASS_SNAKE_CASE_\n"
    "\n"
    "import^godot\n"
    "\n"
    "# Basic plugin template. Worn by the EditorPlugin the editor makes when\n"
    "# the plugin is enabled; what it does is implemented rather than called.\n"
    "@tool\n"
    "public^let^_CLASS_ = def^{\n"
    "_TS_self^{ abstract^gdobj : godot._BASE_ },\n"
    "_TS_override^new = f^obj { self^{ gdobj = obj } },\n"
    "_TS_delegate^self^.gdobj,\n"
    "\n"
    "_TS__enable_plugin = p^self^{\n"
    "_TS__TS_# Add autoloads here.\n"
    "_TS_},\n"
    "\n"
    "_TS__disable_plugin = p^self^{\n"
    "_TS__TS_# Remove autoloads here.\n"
    "_TS_},\n"
    "\n"
    "_TS__enter_tree = p^self^{\n"
    "_TS__TS_# Initialization of the plugin goes here.\n"
    "_TS_},\n"
    "\n"
    "_TS__exit_tree = p^self^{\n"
    "_TS__TS_# Clean-up of the plugin goes here.\n"
    "_TS_},\n"
    "}\n";

const char *const import_script_template =
    "module^_CLASS_SNAKE_CASE_\n"
    "\n"
    "import^godot\n"
    "\n"
    "# Basic import script template.\n"
    "@tool\n"
    "public^let^_CLASS_ = def^{\n"
    "_TS_self^{ abstract^gdobj : godot._BASE_ },\n"
    "_TS_override^new = f^obj { self^{ gdobj = obj } },\n"
    "_TS_delegate^self^.gdobj,\n"
    "\n"
    "_TS_# Called by the editor when a scene has this script set as the import\n"
    "_TS_# script in the import tab.\n"
    "_TS__post_import = f^self^, scene:godot.Node -> godot.Object {\n"
    "_TS__TS_# Modify the contents of the scene upon import.\n"
    "_TS__TS_return^scene  # Return the modified root node when you're done.\n"
    "_TS_},\n"
    "}\n";

const char *const import_script_bare_template =
    "module^_CLASS_SNAKE_CASE_\n"
    "\n"
    "import^godot\n"
    "\n"
    "@tool\n"
    "public^let^_CLASS_ = def^{\n"
    "_TS_self^{ abstract^gdobj : godot._BASE_ },\n"
    "_TS_override^new = f^obj { self^{ gdobj = obj } },\n"
    "_TS_delegate^self^.gdobj,\n"
    "\n"
    "_TS__post_import = f^self^, scene:godot.Node -> godot.Object {\n"
    "_TS__TS_return^scene\n"
    "_TS_},\n"
    "}\n";

const char *const rich_text_effect_template =
    "module^_CLASS_SNAKE_CASE_\n"
    "\n"
    "import^godot\n"
    "\n"
    "# Base template for rich text effects.\n"
    "#\n"
    "# To use this effect:\n"
    "# - Enable BBCode on a RichTextLabel.\n"
    "# - Register this effect on the label.\n"
    "# - Use [_CLASS_SNAKE_CASE_ param=2.0]hello[/_CLASS_SNAKE_CASE_] in text.\n"
    "@tool\n"
    "public^let^_CLASS_ = def^{\n"
    "_TS_self^{\n"
    "_TS__TS_abstract^gdobj : godot._BASE_,\n"
    "_TS__TS_# The label reads this off the instance to know the tag.\n"
    "_TS__TS_bbcode = \"_CLASS_SNAKE_CASE_\",\n"
    "_TS_},\n"
    "_TS_override^new = f^obj { self^{ gdobj = obj } },\n"
    "_TS_delegate^self^.gdobj,\n"
    "\n"
    "_TS__process_custom_fx = f^self^, charFx:godot.CharFXTransform -> bool^{\n"
    "_TS__TS_let^env = charFx.get_environment()\n"
    "_TS__TS_let^param = if^env.has(\"param\"): env.at(\"param\") el^: 1.0;\n"
    "_TS__TS_return^true^\n"
    "_TS_},\n"
    "}\n";

const char *const visual_shader_node_template =
    "module^_CLASS_SNAKE_CASE_\n"
    "\n"
    "import^godot\n"
    "\n"
    "# Visual shader's node plugin template. A port type is\n"
    "# VisualShaderNode.PortType, and PORT_TYPE_SCALAR is 0.\n"
    "@tool\n"
    "public^let^_CLASS_ = def^{\n"
    "_TS_self^{ abstract^gdobj : godot._BASE_ },\n"
    "_TS_override^new = f^obj { self^{ gdobj = obj } },\n"
    "_TS_delegate^self^.gdobj,\n"
    "\n"
    "_TS__get_name = f^self^ -> string^{ return^\"_CLASS_\" },\n"
    "_TS__get_category = f^self^ -> string^{ return^\"\" },\n"
    "_TS__get_description = f^self^ -> string^{ return^\"\" },\n"
    "_TS__get_return_icon_type = f^self^ -> number^{ return^0 },\n"
    "\n"
    "_TS__get_input_port_count = f^self^ -> number^{ return^0 },\n"
    "_TS__get_input_port_name = f^self^, port:number^ -> string^{ return^\"\" },\n"
    "_TS__get_input_port_type = f^self^, port:number^ -> number^{ return^0 },\n"
    "\n"
    "_TS__get_output_port_count = f^self^ -> number^{ return^1 },\n"
    "_TS__get_output_port_name = f^self^, port:number^ -> string^{ return^\"result\" },\n"
    "_TS__get_output_port_type = f^self^, port:number^ -> number^{ return^0 },\n"
    "\n"
    "_TS__get_code = f^self^, inputVars:godot.ArrayOfString, outputVars:godot.ArrayOfString,\n"
    "_TS__TS__TS_mode:number^, type:number^ -> string^{\n"
    "_TS__TS_return^$\"{outputVars.at(1)} = 0.0;\"\n"
    "_TS_},\n"
    "}\n";

// Godot asks per class up the chosen base's chain, so a row kept under Object
// is offered whatever the base. The editor script is kept there on purpose:
// it names no base at all -- a .lh with no module^ is one (05 の 3.2), and
// nothing wears it -- so there is no reason to make a writer find
// EditorScript in the class tree first. GDScript cannot do this, since its
// template has to say `extends EditorScript`.
const BuiltInTemplate built_in_templates[] = {
    {"Node", "Node script", "A class a node wears, and the marks that say so",
     node_template},
    {"Resource", "Custom resource",
     "Data the inspector edits and a .tres keeps", resource_template},
    {"Object", "Module", "A unit require^ reaches. Nothing wears it",
     module_template},
    {"Object", "Editor script",
     "Statements File > Run runs, once. Ignores the base",
     editor_script_template},
    {"CharacterBody2D", "Basic movement",
     "Classic movement for gravity games (platformer, ...)",
     character_body_2d_template},
    {"CharacterBody3D", "Basic movement",
     "Classic movement for gravity games (FPS, TPS, ...)",
     character_body_3d_template},
    {"EditorPlugin", "Plugin", "Basic plugin template",
     editor_plugin_template},
    {"EditorScenePostImport", "Basic import script",
     "Basic import script template", import_script_template},
    {"EditorScenePostImport", "No comments",
     "Basic import script template (no comments)",
     import_script_bare_template},
    {"RichTextEffect", "Default", "Base template for rich text effects",
     rich_text_effect_template},
    {"VisualShaderNodeCustom", "Basic", "Visual shader's node plugin template",
     visual_shader_node_template},
};

}  // namespace

// The four spellings GDScript replaces (gdscript_editor.cpp's make_template),
// and no fifth: a template written for one language then reads in the other.
//
// _CLASS_SNAKE_CASE_ is what the module^ is named. The dialog hands over the
// file's basename and no path at all (script_create_dialog.cpp), so a name of
// one part is as much as can honestly be written -- 05 の 5.3 has the writer
// spell out the rest.
//
// An empty template is the writer having turned templates off, and an empty
// .lh is an editor script with nothing in it. GDScript answers the same way.
Ref<Script> LhatLanguage::_make_template(const String &tmpl,
                                         const String &class_name,
                                         const String &base_class_name) const
{
    Ref<LhatScript> script;
    script.instantiate();
    // _CLASS_SNAKE_CASE_ before _CLASS_, or the longer one is never seen.
    String written = tmpl.replace("_BASE_", base_class_name)
                         .replace("_CLASS_SNAKE_CASE_", class_name.to_snake_case())
                         .replace("_CLASS_", class_name.to_pascal_case())
                         .replace("_TS_", host::indentation());
    script->set_source_code(written);
    return script;
}

// Every one of the six keys has to be there: script_language_extension.h
// drops a row that is missing any of them, and says nothing about it.
// `id` is the dialog's own numbering and is written over; `origin` is 0,
// TemplateLocation::TEMPLATE_BUILT_IN.
TypedArray<Dictionary> LhatLanguage::_get_built_in_templates(
    const StringName &object) const
{
    TypedArray<Dictionary> out;
    for (const BuiltInTemplate &one : built_in_templates) {
        if (String(object) != String(one.inherit)) {
            continue;
        }
        Dictionary row;
        row["inherit"] = String(one.inherit);
        row["name"] = String(one.name);
        row["description"] = String(one.description);
        // String::utf8, not String: the one-argument constructor reads its
        // bytes as Latin-1, and a section number written 05 の 3.2 comes out
        // mangled.
        row["content"] = String::utf8(one.content);
        row["id"] = 0;
        row["origin"] = 0;
        out.push_back(row);
    }
    return out;
}

bool LhatLanguage::_is_using_templates()
{
    return true;
}

PackedStringArray LhatLanguage::_get_reserved_words() const
{
    static const LhatWordKind kinds[] = {LHAT_WORD_CONTROL, LHAT_WORD_DECLARE,
                                         LHAT_WORD_TYPE, LHAT_WORD_VALUE,
                                         LHAT_WORD_CLAUSE};
    PackedStringArray out;
    for (LhatWordKind kind : kinds) {
        for (const char *const *word = lhat_words_of(kind); *word != nullptr;
             word++) {
            out.push_back(unhatted(*word));
        }
    }
    return out;
}

// Asked of what the list above answered, so it is asked without the hat too.
bool LhatLanguage::_is_control_flow_keyword(const String &keyword) const
{
    for (const char *const *word = lhat_words_of(LHAT_WORD_CONTROL);
         *word != nullptr; word++) {
        if (keyword == unhatted(*word)) {
            return true;
        }
    }
    return false;
}

PackedStringArray LhatLanguage::_get_comment_delimiters() const
{
    PackedStringArray out;
    out.push_back("#");
    out.push_back("#[ ]#");  // start and end, separated by a space
    return out;
}

PackedStringArray LhatLanguage::_get_doc_comment_delimiters() const
{
    return PackedStringArray();
}

PackedStringArray LhatLanguage::_get_string_delimiters() const
{
    PackedStringArray out;
    // 01 の 5.3: '"""' runs to the end of the line and writes no terminator,
    // so the end is empty. Measured: the editor takes the longest opening it
    // finds whatever order these arrive in, so this does not have to lead.
    out.push_back("\"\"\" ");
    out.push_back("\" \"");
    out.push_back("' '");
    // 01 の 3.1: a name written in backticks. Not a string, but delimited the
    // same way and worth telling from the code around it.
    out.push_back("` `");
    return out;
}

bool LhatLanguage::_has_named_classes() const
{
    return false;
}

bool LhatLanguage::_can_inherit_from_file() const
{
    return false;
}

// 05 の 5.1 makes a unit a file. There is no form for one kept inside a
// scene, so the dialog's "built-in script" stays greyed out.
bool LhatLanguage::_supports_builtin_mode() const
{
    return false;
}

// 01 の 6.4: L^ has no spelling for a description -- every comment is kept
// alike and the block above a thing is what it says about it, the way Go has
// it. So there is documentation to show, and this is the gate: a language
// answering false is never asked for any.
bool LhatLanguage::_supports_documentation() const
{
    return true;
}

// Asked before the editor will use _get_global_class_name at all: a language
// answering false is never scanned for the names it declares. The type is
// the base a caller is asking about, and what is answered is about this
// language rather than about that type -- a .lh names a class the same way
// whatever it is asked in aid of.
bool LhatLanguage::_handles_global_class_type(const String &type) const
{
    return type == _get_type();
}

// Asked of a path rather than of a loaded script: the editor scans the
// project for what each file declares before it opens any of them, and this
// is what puts a class in the Create Node dialog and hangs @icon's icon on
// it.
//
// A .lh has to be read and run to answer, since 05 の 5.5 has the classes a
// unit publishes come from the table it answers with -- there is no reading
// the tree for them. So one script is made, reloaded and thrown away. That
// is what GDScript's own scan costs too, a parse per file.
Dictionary LhatLanguage::_get_global_class_name(const String &path) const
{
    Dictionary out;
    Ref<FileAccess> reading = FileAccess::open(path, FileAccess::READ);
    if (reading.is_null()) {
        return out;
    }

    Ref<LhatScript> probe;
    probe.instantiate();
    // The path before the source: a require^ resolves against it, and the
    // reload below is what follows one. Written past the resource cache --
    // set_path would claim a path the real script may already hold, and two
    // resources answering one path is what that refusal is for.
    probe->set_path_cache(path);
    probe->_set_source_code(reading->get_as_text());
    probe->_reload(false);

    StringName named = probe->_get_global_name();
    if (named == StringName()) {
        return out;  // a library, or a name the engine already has
    }
    out["name"] = named;
    out["base_type"] = probe->_get_instance_base_type();
    String icon = probe->lhat_icon_path();
    if (!icon.is_empty()) {
        out["icon_path"] = icon;
    }
    return out;
}

ScriptLanguage::ScriptNameCasing LhatLanguage::_preferred_file_name_casing()
    const
{
    return ScriptLanguage::SCRIPT_NAME_CASING_SNAKE_CASE;
}

// Both of these answer a dictionary the engine takes apart, and the key it
// reads first is "result". An empty one is not "nothing to offer" but a
// malformed answer: script_language_extension.h refuses it and logs, which
// the editor does on every keystroke a completion could follow.
//
// So the shape is written out, with the answer being that nothing was found.
// 07 の lsp/ already knows how to complete a name; _complete_code is where
// that would be hooked up, and until it is this is what "no" looks like.
Dictionary LhatLanguage::_complete_code(const String &code, const String &path,
                                        Object *owner) const
{
    (void)code;
    (void)path;
    (void)owner;
    Dictionary out;
    out["result"] = (int)OK;
    out["force"] = false;
    out["call_hint"] = String();
    out["options"] = Array();
    return out;
}

Dictionary LhatLanguage::_lookup_code(const String &code, const String &symbol,
                                      const String &path, Object *owner) const
{
    (void)code;
    (void)symbol;
    (void)path;
    (void)owner;
    Dictionary out;
    out["result"] = (int)ERR_UNAVAILABLE;  // nothing to jump to, not an error
    // Read whether the result said anything was found, so it has to be there
    // even when nothing was.
    out["type"] = (int)LOOKUP_RESULT_MAX;
    return out;
}

String LhatLanguage::_auto_indent_code(const String &code, int32_t from_line,
                                       int32_t to_line) const
{
    (void)from_line;
    (void)to_line;
    return code;
}

namespace {

// The name a line binds, and what it binds it to. One pass over the text,
// which is what both the outline and the jump-to-member want.
//
// Read off the text rather than the tree, for two reasons. The line numbers
// have to agree with the buffer being shown, which the tree's do not while a
// unit is being edited; and the editor asks with the buffer in hand rather
// than the file, sometimes for a script that has never checked.
struct Bound {
    String name;
    String value;  // what stands after the '=' or ':=', stripped
    bool found = false;
};

// 8.6 and 14.12: what may lead a name before it is the name. The introducers
// take a space or none (01 の 2.3 makes the hat part of the word, so
// `public^let^Spinner` is three words with nothing between them), and the
// three markers of 14.12 stand in the same place on a member.
const char *const leading[] = {"public^",   "let^",      "var^",
                               "override^", "overload^", "abstract^"};

// Where the name ends. 01 の 2.3 keeps the hat in, and everything else that
// can follow a name closes it.
bool ends_name(char32_t c)
{
    return c == ' ' || c == '\t' || c == '=' || c == ':' || c == ',' ||
           c == '(' || c == '{' || c == '#';
}

Bound bound_on(const String &line)
{
    Bound out;
    String bare = line.strip_edges();
    // 02 の 18.6: an annotation is written above the declaration it is about,
    // and "above" is as often the same line. It is not the name, so it comes
    // off -- with its arguments, which 18.3 keeps to literals and so to one
    // pair of brackets.
    while (bare.begins_with("@")) {
        int at = 1;
        while (at < bare.length() && !ends_name(bare[at])) {
            at++;
        }
        if (at < bare.length() && bare[at] == '(') {
            int depth = 0;
            while (at < bare.length()) {
                if (bare[at] == '(') {
                    depth++;
                } else if (bare[at] == ')' && --depth == 0) {
                    at++;
                    break;
                }
                at++;
            }
        }
        String left = bare.substr(at).strip_edges();
        if (left == bare) {
            return out;  // nothing came off; an annotation and nothing else
        }
        bare = left;
    }
    bool again = true;
    while (again) {
        again = false;
        for (const char *word : leading) {
            if (bare.begins_with(word)) {
                bare = bare.substr((int)strlen(word)).strip_edges();
                again = true;
                break;
            }
        }
    }

    int at = 0;
    while (at < bare.length() && !ends_name(bare[at])) {
        at++;
    }
    if (at == 0) {
        return out;
    }
    String name = bare.substr(0, at);
    String after = bare.substr(at).strip_edges();
    // 14.14改2: '=' is the recommended spelling and ':=' reads the same.
    if (after.begins_with(":=")) {
        out.value = after.substr(2).strip_edges();
    } else if (after.begins_with("=")) {
        out.value = after.substr(1).strip_edges();
    } else {
        return out;
    }
    out.name = name;
    out.found = true;
    return out;
}

}  // namespace

// Which line writes this member, 1-based, or -1. What a jump-to-member lands
// on, and what the editor asks when it looks for a function by name.
int32_t lhat_member_line(const String &code, const String &member)
{
    PackedStringArray lines = code.split("\n");
    for (int64_t i = 0; i < lines.size(); i++) {
        Bound said = bound_on(lines[i]);
        if (said.found && said.name == member) {
            return (int32_t)(i + 1);
        }
    }
    return -1;
}

// What the script editor lists down the side of the code (its members
// overview), as "name:line" with the line 1-based -- the shape
// ScriptLanguageExtension::validate reads out of the "functions" key.
//
// Subroutines only, which is the same choice GDScript makes: its outline is
// the file's func declarations. A def^ member bound to anything else is a
// field or a static constant (14.7改), and the inspector is where those are
// read.
PackedStringArray lhat_subroutine_lines(const String &code)
{
    PackedStringArray out;
    PackedStringArray lines = code.split("\n");
    for (int64_t i = 0; i < lines.size(); i++) {
        Bound said = bound_on(lines[i]);
        if (said.found &&
            (said.value.begins_with("p^") || said.value.begins_with("f^"))) {
            out.push_back(said.name + ":" + String::num_int64(i + 1));
        }
    }
    return out;
}

int32_t LhatLanguage::_find_function(const String &function,
                                     const String &code) const
{
    return lhat_member_line(code, function);
}

// Nothing, on purpose, and the only place in the engine that asks is
// script_text_editor.cpp's add_callback -- which puts the answer at the very
// end of the file and nowhere else. A member written there falls outside the
// def^'s closing brace, so what would come back from here could not be right
// wherever it were put. C# reached the same conclusion
// (csharp_script.cpp: "The make_function() API does not work for C#
// scripts") and stopped at refusing the feature.
//
// This goes one further: _can_make_function stays true, so the connect
// dialog does not warn about something that is in fact done, and what the
// editor appends is the two newlines around this empty answer -- which
// leaves the file it saves a moment later sound. The member itself is
// written by lhat_receiver_write, in the body, from the plugin.
String LhatLanguage::_make_function(const String &class_name,
                                    const String &function_name,
                                    const PackedStringArray &args) const
{
    (void)class_name;
    (void)function_name;
    (void)args;
    return String();
}

bool LhatLanguage::_can_make_function() const
{
    return true;
}

bool LhatLanguage::_overrides_external_editor()
{
    return false;
}

void LhatLanguage::_add_global_constant(const StringName &name,
                                        const Variant &value)
{
    (void)name;
    (void)value;
}

void LhatLanguage::_add_named_global_constant(const StringName &name,
                                              const Variant &value)
{
    (void)name;
    (void)value;
}

void LhatLanguage::_remove_named_global_constant(const StringName &name)
{
    (void)name;
}

void LhatLanguage::_thread_enter()
{
}

void LhatLanguage::_thread_exit()
{
}

void LhatLanguage::_frame()
{
}

// The three ways the engine asks for a reload. Script::reload_from_file no
// longer reads the file itself -- it hands the question here -- so reading
// it is part of the answer (lhat_script.cpp's reload_from_disk).

// A .lh changed on disk while nothing named it. Every one this program holds
// is asked, since there is no telling which the change was.
// One rebuild, not one per script. Reading the text is per file; making the
// world again is not, so the sources are taken first and the world once --
// otherwise asking for twenty scripts would build twenty worlds and throw
// away nineteen.
void LhatLanguage::_reload_all_scripts()
{
    LocalVector<Ref<Script>> held;
    for (LhatScript *const &script : LhatScript::all()) {
        held.push_back(Ref<Script>(script));
    }
    LocalVector<LhatScript *> changed;
    for (const Ref<Script> &script : held) {
        LhatScript *ours = Object::cast_to<LhatScript>(script.ptr());
        ours->read_source_from_disk();
        changed.push_back(ours);
    }
    rebuild_world(changed, true, true);
}

void LhatLanguage::_reload_scripts(const Array &scripts, bool soft_reload)
{
    (void)soft_reload;
    LocalVector<LhatScript *> changed;
    for (int64_t i = 0; i < scripts.size(); i++) {
        Ref<Script> held = scripts[i];
        LhatScript *ours = Object::cast_to<LhatScript>(held.ptr());
        if (ours != nullptr) {
            ours->read_source_from_disk();
            changed.push_back(ours);
        }
    }
    rebuild_world(changed, false, true);
}

// 02 の 18: what a @tool class carries is that it runs while a scene is being
// edited, so this is the same question asked while it is running.
void LhatLanguage::_reload_tool_script(const Ref<Script> &script,
                                       bool soft_reload)
{
    LhatScript *ours = Object::cast_to<LhatScript>(script.ptr());
    if (ours != nullptr) {
        ours->reload_from_disk(soft_reload);
    }
}

TypedArray<Dictionary> LhatLanguage::_get_public_functions() const
{
    return TypedArray<Dictionary>();
}

Dictionary LhatLanguage::_get_public_constants() const
{
    return Dictionary();
}

TypedArray<Dictionary> LhatLanguage::_get_public_annotations() const
{
    return TypedArray<Dictionary>();
}

void LhatLanguage::_profiling_start()
{
}

void LhatLanguage::_profiling_stop()
{
}

void LhatLanguage::_profiling_set_save_native_calls(bool enable)
{
    (void)enable;
}

int32_t LhatLanguage::_profiling_get_accumulated_data(
    ScriptLanguageExtensionProfilingInfo *info, int32_t max)
{
    (void)info;
    (void)max;
    return 0;
}

int32_t LhatLanguage::_profiling_get_frame_data(
    ScriptLanguageExtensionProfilingInfo *info, int32_t max)
{
    (void)info;
    (void)max;
    return 0;
}

// 04 の 11.6改 through the engine's debugger panel. What every one of these
// answers is the fault host::run_problem copied out when it happened, not
// the machine's own frames: those are readable only until the next run, and
// the panel asks when a person opens it.
//
// `level` counts from the innermost frame, which is the order the panel
// wants and the order lhat_machine_fault_frame walks.
String LhatLanguage::_debug_get_error() const
{
    return host::last_fault();
}

int32_t LhatLanguage::_debug_get_stack_level_count() const
{
    return host::frame_count();
}

int32_t LhatLanguage::_debug_get_stack_level_line(int32_t level) const
{
    host::FaultFrame one;
    return host::frame_at(level, &one) ? one.line : 0;
}

String LhatLanguage::_debug_get_stack_level_function(int32_t level) const
{
    host::FaultFrame one;
    return host::frame_at(level, &one) ? one.name : String();
}

String LhatLanguage::_debug_get_stack_level_source(int32_t level) const
{
    host::FaultFrame one;
    return host::frame_at(level, &one) ? one.source : String();
}

// 09 の 3.2: the frame's own bindings and what it captured. Only while the
// machine is stopped in the hook -- a fault's registers are gone by the time
// the panel asks, and only a live frame has any.
Dictionary LhatLanguage::_debug_get_stack_level_locals(int32_t level,
                                                       int32_t max_subitems,
                                                       int32_t max_depth)
{
    // Depth is what a value is walked to when it is a structure, and a
    // binding is written out whole here rather than walked.
    (void)max_subitems;
    (void)max_depth;
    return host::frame_locals(level);
}

// 14.3's fields of whatever self^ the frame is a method of.
Dictionary LhatLanguage::_debug_get_stack_level_members(int32_t level,
                                                        int32_t max_subitems,
                                                        int32_t max_depth)
{
    (void)max_subitems;
    (void)max_depth;
    return host::frame_members(level);
}

// The engine's own `evaluate` will not run without this (remote_debugger.cpp
// bails on a null one), and the inspector shows the object it names.
void *LhatLanguage::_debug_get_stack_level_instance(int32_t level)
{
    return host::frame_instance(level);
}

Dictionary LhatLanguage::_debug_get_globals(int32_t max_subitems,
                                            int32_t max_depth)
{
    (void)max_subitems;
    (void)max_depth;
    return Dictionary();
}

// The same frames as one array, which is what the panel reads to draw the
// list rather than asking level by level. The three keys are the ones
// ScriptDebugger looks for.
// Required, and the one door an L^ expression fits through: Godot's own
// `evaluate` never asks a language to evaluate anything -- it takes the
// locals and runs them through its Expression. This is what the local
// debugger's `p` calls.
String LhatLanguage::_debug_parse_stack_level_expression(
    int32_t level, const String &expression, int32_t max_subitems,
    int32_t max_depth)
{
    (void)max_subitems;
    (void)max_depth;
    return host::frame_evaluate(level, expression);
}

TypedArray<Dictionary> LhatLanguage::_debug_get_current_stack_info()
{
    TypedArray<Dictionary> out;
    const int32_t deep = host::frame_count();
    for (int32_t at = 0; at < deep; at++) {
        host::FaultFrame one;
        if (!host::frame_at(at, &one)) {
            break;
        }
        Dictionary said;
        said["file"] = one.source;
        said["func"] = one.name;
        said["line"] = one.line;
        out.push_back(said);
    }
    return out;
}

}  // namespace godot
