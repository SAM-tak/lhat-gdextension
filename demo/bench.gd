extends SceneTree

# What one engine call costs from L^, against what it costs from GDScript
# with the type known -- which is the dispatch this is all measured against.

const TIMES := 200000

func gdscript_set(node: Node2D, times: int) -> void:
	var turn := 0.0
	for i in times:
		turn += 0.001
		node.set_rotation(turn)

func gdscript_get(node: Node2D, times: int) -> float:
	var sum := 0.0
	for i in times:
		sum += node.get_rotation()
	return sum

func gdscript_spin(times: int) -> void:
	var turn := 0.0
	for i in times:
		turn += 0.001

func _initialize() -> void:
	var said: PackedStringArray = LhatRuntime.check("res://bench.lh")
	if not said.is_empty():
		for line in said:
			push_error(line)
		quit(1)
		return

	var node := Sprite2D.new()
	get_root().add_child(node)
	node.set_script(load("res://bench.lh"))
	await process_frame

	var names := ["spin", "callLhat", "valid", "setByName", "setDirect", "setDelegated",
			"getDirect", "getDelegated"]
	for name in names:
		node.call(name, 1000)
	gdscript_spin(1000)
	gdscript_set(node, 1000)
	gdscript_get(node, 1000)

	for pass_index in range(3):
		var timings := {}
		for name in names:
			var before := Time.get_ticks_usec()
			node.call(name, TIMES)
			timings[name] = Time.get_ticks_usec() - before

		var t := Time.get_ticks_usec()
		gdscript_spin(TIMES)
		timings["gd:spin"] = Time.get_ticks_usec() - t
		t = Time.get_ticks_usec()
		gdscript_set(node, TIMES)
		timings["gd:set"] = Time.get_ticks_usec() - t
		t = Time.get_ticks_usec()
		gdscript_get(node, TIMES)
		timings["gd:get"] = Time.get_ticks_usec() - t

		print("pass ", pass_index)
		for name in timings:
			print("   ", name, ": ", timings[name], "us")
	quit(0)
