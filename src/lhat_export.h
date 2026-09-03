// L^ (lhat) -- what an export does with .lh files, and with the library.
//
// 05 の 10 章: a unit checked and compiled once may go out as bytes, and a
// core built without its front end (10.8) reads nothing else. Which of the
// two an export carries is a choice the preset holds, the way GDScript's own
// "Script" export mode is -- but that one is the engine's, and a language
// from outside adds its own through an EditorExportPlugin.
//
// The library is chosen by the engine, from the .gdextension rows and the
// features the preset has -- what this plugin raises (_get_export_features)
// reaches only the exported game. So the plain template rows name the
// VM-only library, and Text is the mode that ships one more: the library
// with the front end, under a row with the `lhat_text` tag on top, which
// wins in the game by having the more tags.
//
// Editor-only: registered at MODULE_INITIALIZATION_LEVEL_EDITOR and put on
// the editor by LhatEditorPlugin (lhat_highlighter.h).

#ifndef LHAT_GODOT_EXPORT_H
#define LHAT_GODOT_EXPORT_H

#include <godot_cpp/classes/editor_export_platform.hpp>
#include <godot_cpp/classes/editor_export_plugin.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/typed_array.hpp>

#include "lhat.h"
#include "lhat_host.h"

namespace godot {

class LhatExportPlugin : public EditorExportPlugin {
    GDCLASS(LhatExportPlugin, EditorExportPlugin)

    // lhat/script_export_mode, as the preset holds it.
    enum Mode { MODE_TEXT = 0, MODE_COMPILED = 1 };
    bool compiled() const;

    // The program the units go through in Compiled mode: one per export,
    // reading the files as they stand on disk rather than the editor's
    // buffers, and freed with it. `units` has to outlive it.
    host::Units units;
    LhatProgram *program = nullptr;
    bool debug = false;     // 09 の 4 章: keep the names a debugger reads
    bool reported = false;  // the checker's lines, written once

    void compile_unit(const String &path);
    void add_text_library(const String &path,
                          const PackedStringArray &features);

protected:
    static void _bind_methods();

public:
    String _get_name() const override;
    bool _supports_platform(
        const Ref<EditorExportPlatform> &platform) const override;
    TypedArray<Dictionary> _get_export_options(
        const Ref<EditorExportPlatform> &platform) const override;
    PackedStringArray _get_export_features(
        const Ref<EditorExportPlatform> &platform, bool is_debug) const override;
    void _export_begin(const PackedStringArray &features, bool is_debug,
                       const String &path, uint32_t flags) override;
    void _export_file(const String &path, const String &type,
                      const PackedStringArray &features) override;
    void _export_end() override;
};

}  // namespace godot

#endif  // LHAT_GODOT_EXPORT_H
