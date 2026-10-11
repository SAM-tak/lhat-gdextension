extends SceneTree

# Called separately by scripts/smoke.sh. Invalid units live in user:// so
# the editor does not import them as project scripts.

func write_unit(path: String, source: String) -> void:
	var file := FileAccess.open(path, FileAccess.WRITE)
	file.store_string(source)
	file.close()

func _initialize() -> void:
	var checks := {
		"valid": ["import^godot\ngodot.defer(p^{ print(\"queued\") })\n", false],
		"parameters": ["import^godot\ngodot.defer(p^x:number^ {})\n", true],
		"function": ["import^godot\ngodot.defer(f^{ return^1 })\n", true],
		"captured_call_types": ["import^godot\nlet^say = p^text:string^ {}\ngodot.defer(p^{ say(42) })\n", true],
	}
	for name in checks:
		var path: String = "user://defer-check-" + name + ".lh"
		write_unit(path, checks[name][0])
		var errors := LhatRuntime.check(path)
		if errors.is_empty() == checks[name][1]:
			push_error("unexpected defer type-check result: " + name + " " + str(errors))
			quit(1)
			return
		print("type check ", name, ": OK")
		DirAccess.remove_absolute(path)
	write_unit("user://defer-cancel-run.lh", "import^godot\ngodot.defer(p^{ print(\"DEFER_SHOULD_NOT_RUN\") })\n")
	var errors := LhatRuntime.run("user://defer-cancel-run.lh")
	if not errors.is_empty():
		push_error(str(errors))
		quit(1)
		return
	write_unit("user://defer-cancel-call.lh", "module^defer_cancel\nimport^godot\npublic^let^queue = p^{ godot.defer(p^{ print(\"DEFER_SHOULD_NOT_RUN\") }) }\n")
	LhatRuntime.call_member("user://defer-cancel-call.lh", "queue")
	await process_frame
	await process_frame
	DirAccess.remove_absolute("user://defer-cancel-run.lh")
	DirAccess.remove_absolute("user://defer-cancel-call.lh")
	print("defer checks: OK")
	quit(0)
