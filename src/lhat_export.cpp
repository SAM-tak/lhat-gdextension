#include "lhat_export.h"

#include <string.h>

#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

namespace {

// The option as the preset spells it; the word before the slash is the
// group the export dialog shows it under.
const char *const OPTION = "lhat/script_export_mode";

// The tag a Text export raises on the game. With it, the .gdextension row
// naming the library with the front end has one tag more than the plain row
// and wins (GDExtensionLibraryLoader::find_extension_library).
const char *const TEXT_TAG = "lhat_text";

// What the core lhat_alloc'd, as the engine's bytes.
PackedByteArray taken(uint8_t *bytes, size_t length)
{
    PackedByteArray out;
    out.resize((int64_t)length);
    memcpy(out.ptrw(), bytes, length);
    lhat_free(bytes);
    return out;
}

}  // namespace

void LhatExportPlugin::_bind_methods()
{
}

String LhatExportPlugin::_get_name() const
{
    return "L^";
}

bool LhatExportPlugin::_supports_platform(
    const Ref<EditorExportPlatform> &platform) const
{
    (void)platform;
    return true;
}

TypedArray<Dictionary> LhatExportPlugin::_get_export_options(
    const Ref<EditorExportPlatform> &platform) const
{
    (void)platform;
    Dictionary option;
    option["name"] = OPTION;
    option["type"] = Variant::INT;
    option["hint"] = PROPERTY_HINT_ENUM;
    option["hint_string"] = "Text,Compiled";
    Dictionary row;
    row["option"] = option;
    row["default_value"] = MODE_COMPILED;
    TypedArray<Dictionary> out;
    out.push_back(row);
    return out;
}

bool LhatExportPlugin::compiled() const
{
    // A preset written before the option existed holds nothing under the
    // name, and is read as the default.
    Variant chosen = get_option(OPTION);
    return chosen.get_type() == Variant::NIL || (int)chosen != MODE_TEXT;
}

PackedStringArray LhatExportPlugin::_get_export_features(
    const Ref<EditorExportPlatform> &platform, bool is_debug) const
{
    (void)platform;
    (void)is_debug;
    PackedStringArray out;
    if (!compiled()) {
        out.push_back(TEXT_TAG);
    }
    return out;
}

void LhatExportPlugin::_export_begin(const PackedStringArray &features,
                                     bool is_debug, const String &path,
                                     uint32_t flags)
{
    (void)features;
    (void)path;
    (void)flags;
    debug = is_debug;
    reported = false;
    if (!compiled()) {
        return;
    }
    units = host::units_for("res://");
    program = host::program_for(&units);
    if (program == nullptr) {
        UtilityFunctions::push_error(
            String("L^: out of memory making the program for the export"));
        return;
    }
    // 05 の 10.7: the table holds what install would build for every
    // signature the host registered -- which program_for has just done --
    // and a library without the front end reads it before registering the
    // same names (lhat_host.cpp). Written here, before any unit goes
    // through the program.
    uint8_t *bytes = nullptr;
    size_t length = 0;
    if (!lhat_program_write_signatures(program, &bytes, &length)) {
        UtilityFunctions::push_error(
            String("L^: the signature table could not be written"));
        return;
    }
    add_file(host::SIGNATURES_PATH, host::packed(taken(bytes, length)), false);
}

void LhatExportPlugin::_export_file(const String &path, const String &type,
                                    const PackedStringArray &features)
{
    if (type == "GDExtension") {
        if (!compiled()) {
            add_text_library(path, features);
        }
        return;
    }
    if (program != nullptr && path.get_extension().to_lower() == "lh") {
        compile_unit(path);
    }
}

// 05 の 10 章: the unit as bytes, under the same path. A require^ inside
// the bytes names the file as it was written, and the loader reads the .pck
// under that name (lhat_host.cpp) -- so nothing is renamed or remapped. The
// bytes go in under the zstd wrapper the loader takes off (host::packed).
void LhatExportPlugin::compile_unit(const String &path)
{
    CharString named = host::unit_path(units, path).utf8();
    const LhatUnit *unit = lhat_program_check(program, named.get_data());
    if (unit == nullptr || lhat_program_has_errors(program)) {
        // Left as text. A plugin cannot fail an export, and a VM-only
        // library refuses text aloud at start-up (10.8), which is louder
        // than an export that quietly went out short of a unit.
        if (!reported) {
            PackedStringArray said;
            host::diagnostics_into(program, said);
            for (int i = 0; i < said.size(); i++) {
                UtilityFunctions::push_error(said[i]);
            }
            reported = true;
        }
        return;
    }
    if (!lhat_program_compile(program)) {
        UtilityFunctions::push_error(host::compile_failure(program, path));
        return;
    }
    uint8_t *bytes = nullptr;
    size_t length = 0;
    if (!lhat_unit_write_binary(unit, debug, &bytes, &length)) {
        UtilityFunctions::push_error(
            host::problem(path, "could not be written out as a compiled unit"));
        return;
    }
    add_file(path, host::packed(taken(bytes, length)), false);
    skip();
}

// The engine has already shipped the library the plain row names for these
// features. The row with the text tag on top of the same features names the
// one with the front end -- one per architecture the preset builds for, as
// the engine does it.
void LhatExportPlugin::add_text_library(const String &path,
                                       const PackedStringArray &features)
{
    Ref<ConfigFile> config;
    config.instantiate();
    if (config->load(path) != OK ||
        String(config->get_value("configuration", "entry_symbol", "")) !=
            "lhat_library_init") {
        return;  // another extension's
    }
    PackedStringArray keys = config->get_section_keys("libraries");
    for (int i = 0; i < keys.size(); i++) {
        PackedStringArray tags = keys[i].split(".");
        bool tagged = false;
        bool holds = true;
        for (int j = 0; j < tags.size() && holds; j++) {
            if (tags[j] == TEXT_TAG) {
                tagged = true;
            } else {
                holds = features.has(tags[j]);
            }
        }
        if (tagged && holds) {
            add_shared_object(String(config->get_value("libraries", keys[i])),
                              tags, "");
        }
    }
}

void LhatExportPlugin::_export_end()
{
    if (program != nullptr) {
        lhat_program_free(program);
        program = nullptr;
    }
    units.held.clear();
}

}  // namespace godot
