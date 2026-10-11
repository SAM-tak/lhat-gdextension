extends Node

# The whole demo, in the one place that runs both ways: F5 in the editor and
#
#     godot --headless --path godot/demo
#
# An EditorScript would not do -- File > Run wants an EditorScript and
# --script wants a MainLoop, and one script cannot be both. A main scene is
# what both of them already know how to start.
#
# Exits non-zero if the extension did not load or the unit did not run, so it
# is the form that suits a build that wants checking rather than looking at.
func _ready() -> void:
	if not ClassDB.class_exists("LhatRuntime"):
		push_error("the L^ extension did not load")
		get_tree().quit(1)
		return

	print("L^ ", LhatRuntime.version())

	# Empty means the whole graph -- hello.lh and what it requires -- read,
	# parsed and type checked without one diagnostic.
	var said := LhatRuntime.check("res://hello.lh")
	if not said.is_empty():
		for line in said:
			push_error(line)
		get_tree().quit(1)
		return

	# The same, and then runs it. What it prints goes to the Output panel.
	said = LhatRuntime.run("res://hello.lh")
	for line in said:
		push_error(line)
	if not said.is_empty():
		get_tree().quit(1)
		return

	# Values crossing both ways: a unit answers the table of its public^
	# names, and one of them is called with Variants converted on the way in
	# and on the way back.
	var api := "res://lib/api.lh"
	print(LhatRuntime.call_member(api, "greet", ["Godot"]))
	print(LhatRuntime.call_member(api, "total", [[1, 2, 3, 4]]))
	print(LhatRuntime.call_member(api, "describe", ["L^"]))
	print(LhatRuntime.call_member(api, "numbers", [5]))

	# Both timeout() accessors must return the signal of their own receiver.
	var signal_probe := Node.new()
	signal_probe.set_script(load("res://counter.lh"))
	var timer := Timer.new()
	var timer_signal = signal_probe.call("timer_signal", timer)
	var scene_timer := get_tree().create_timer(0.01)
	var scene_signal = signal_probe.call("scene_tree_timer_signal", scene_timer)
	var button := Button.new()
	var tween := get_tree().create_tween()
	tween.tween_interval(1.0)
	var signal_node := Label.new()
	var area := Area2D.new()
	var animation := AnimationPlayer.new()
	var signals = signal_probe.call("engine_signals", button, tween, signal_node, area, animation)
	var expected := {
		"pressed": button, "toggled": button, "finished": tween,
		"ready": signal_node, "tree_exited": signal_node, "body_entered": area,
		"animation_finished": animation,
	}
	var signals_ok := signals is Dictionary
	for signal_name in expected:
		var obtained = signals.get(signal_name) if signals is Dictionary else null
		if not obtained is Signal:
			signals_ok = false
		elif obtained.get_name() != StringName(signal_name) or obtained.get_object() != expected[signal_name]:
			signals_ok = false
	# Returning a toggled Signal must preserve its emitted bool argument.
	var toggled_values: Array = []
	if signals_ok:
		signals["toggled"].connect(func(value): toggled_values.append(value))
		button.toggled.emit(true)
		signals_ok = toggled_values == [true]
	button.free()
	tween.kill()
	signal_node.free()
	area.free()
	animation.free()
	signal_probe.free()
	if not signals_ok:
		push_error("engine signal accessors returned an incorrect signal or lost its arguments")
		timer.free()
		get_tree().quit(1)
		return
	if not timer_signal is Signal or not scene_signal is Signal:
		push_error("timeout() did not return a Signal")
		timer.free()
		get_tree().quit(1)
		return
	if timer_signal.get_name() != &"timeout" or timer_signal.get_object() != timer \
			or scene_signal.get_name() != &"timeout" or scene_signal.get_object() != scene_timer:
		push_error("timeout() returned the wrong signal or receiver")
		timer.free()
		get_tree().quit(1)
		return
	timer.free()
	await scene_signal

	# A node wearing an L^ script. The members are shared with every other
	# node wearing the file and the self^ fields are each node's own
	# (02 の 14.3), so two of them count separately.
	var script := load("res://counter.lh")
	var one := Node.new()
	var two := Node.new()
	one.set_script(script)
	two.set_script(script)
	add_child(one)
	add_child(two)

	# _ready is deferred to the frame the node entered on, and _process runs
	# from then on because has_method said the unit wrote one.
	await get_tree().process_frame
	await get_tree().process_frame
	one.call("tick")
	print("one=", one.call("count"), " two=", two.call("count"))
	print(one.call("greet", "Godot"))

	# The other thing a script is worn by: a Resource rather than a Node.
	# Its @export fields are what the inspector edits and what a .tres keeps,
	# which is what makes a custom resource worth writing.
	var item := Resource.new()
	item.set_script(load("res://item.lh"))
	item.set("name", "a lamp")
	item.set("price", 40)
	item.call("raise", 5)
	print(item.call("describe"))

	var where := "user://item.tres"
	if ResourceSaver.save(item, where) != OK:
		push_error("the custom resource would not save")
		get_tree().quit(1)
		return
	var back := ResourceLoader.load(where, "", ResourceLoader.CACHE_MODE_IGNORE)
	if back == null or back.call("describe") != item.call("describe"):
		push_error("the custom resource did not come back as it went")
		get_tree().quit(1)
		return
	print("from the .tres: ", back.call("describe"))

	get_tree().quit(0)
