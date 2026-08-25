extends SceneTree

# What the extension registers, as lhat-host.json (05 の 8.7). The language
# server reads that file to know what the checker was told, and only the
# engine can say it: the registrations happen in the library it loads.
#
#     godot --headless --path demo --script dump_host_api.gd -- <out>
#
# <out> may be an absolute path as well as a res:// or user:// one, which is
# how scripts/dump-host-api.ps1 has this write straight to the workspace root.
func _initialize() -> void:
	var out := "user://lhat-host.json"
	var args := OS.get_cmdline_user_args()
	if args.size() > 0:
		out = args[0]

	var said := LhatRuntime.dump_host_api(out)
	if said != "":
		push_error(said)
		quit(1)
		return
	print(out)
	quit(0)
