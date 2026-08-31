#include "lhat_resource_format.h"

#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>

#include "lhat_script.h"

namespace godot {

void LhatScriptLoader::_bind_methods()
{
}

PackedStringArray LhatScriptLoader::_get_recognized_extensions() const
{
    PackedStringArray out;
    out.push_back("lh");
    return out;
}

bool LhatScriptLoader::_handles_type(const StringName &type) const
{
    return type == StringName("Script") || type == StringName("LhatScript");
}

String LhatScriptLoader::_get_resource_type(const String &path) const
{
    return path.get_extension().to_lower() == "lh" ? "LhatScript" : "";
}

namespace {

// 05 の 5.3 with GDScript's own GDScriptCache: one script per path, held for
// as long as the process runs. Weakly would not do -- Godot's REPLACE modes
// replace what a scene's ext_resource points at, so the last reference to the
// script the old scene was built against goes away in the middle of building
// the new one, while the connections it is restoring still name it. Holding
// it here is what keeps a Script the engine is walking from being freed
// under it, and it is what GDScript does.
HashMap<String, Ref<LhatScript>> &standing()
{
    static HashMap<String, Ref<LhatScript>> it;
    return it;
}

}  // namespace

Variant LhatScriptLoader::_load(const String &path, const String &original_path,
                                bool use_sub_threads,
                                int32_t cache_mode) const
{
    (void)original_path;
    (void)use_sub_threads;
    (void)cache_mode;

    if (!FileAccess::file_exists(path)) {
        return ERR_FILE_NOT_FOUND;
    }

    // 05 の 5.3: one script per path, whatever the cache mode says. Godot's
    // REPLACE modes mean "let this be what the cache answers with", not
    // "throw the old one away" -- a resource lives as long as something
    // holds it, and the nodes of the scene being replaced hold this one. So
    // a second script for one path would stand beside the first, and both
    // would run the unit and take L^.modules from each other: the older
    // one's def^ would be left pointing into a registration that is no
    // longer there. GDScript keeps the same rule through GDScriptCache and
    // reloads in place; this is that rule.
    //
    // What REPLACE actually wants -- the text on disk, picked up -- is done
    // to the one script rather than to a new one.
    Ref<LhatScript> &script = standing()[path];
    if (script.is_null()) {
        script.instantiate();
        script->set_path(path);
    }
    script->set_source_code(FileAccess::get_file_as_string(path));
    // Checking here is what makes the editor's error list right the moment a
    // file is opened, rather than on the first edit.
    script->reload(false);
    return script;
}

void LhatScriptSaver::_bind_methods()
{
}

Error LhatScriptSaver::_save(const Ref<Resource> &resource, const String &path,
                             uint32_t flags)
{
    (void)flags;

    Ref<LhatScript> script = resource;
    if (script.is_null()) {
        return ERR_INVALID_PARAMETER;
    }

    Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
    if (file.is_null()) {
        return FileAccess::get_open_error();
    }
    file->store_string(script->get_source_code());
    file->close();
    return OK;
}

bool LhatScriptSaver::_recognize(const Ref<Resource> &resource) const
{
    return Object::cast_to<LhatScript>(resource.ptr()) != nullptr;
}

PackedStringArray LhatScriptSaver::_get_recognized_extensions(
    const Ref<Resource> &resource) const
{
    PackedStringArray out;
    if (Object::cast_to<LhatScript>(resource.ptr()) != nullptr) {
        out.push_back("lh");
    }
    return out;
}

}  // namespace godot
