// L^ (lhat) -- Godot's Array and Dictionary, as 05 の 8.8's host data.
//
// L^ has a table, and a table is the better shape for both -- typed, and the
// language's own. So the temptation is to convert at the boundary. What that
// costs is a Variant per element per call, on a road a game walks every
// frame, and a thousand-element answer nobody reads all of still pays for a
// thousand.
//
// A view is what would answer that, and Godot has nowhere to put one: Array
// is one pointer to an ArrayPrivate holding a Vector<Variant>, Dictionary one
// to a HashMap, and neither declares a single virtual. operator[] reads the
// storage outright. There is no seam.
//
// So these are handles, like the packed arrays beside them, and the element
// crosses only when it is read.
//
// What is bought back is the typing, through 8.8改 rather than through
// conversion. TypedArray<T> derives from Array and adds no member and no
// virtual -- the element type lives in ArrayPrivate::typed, a run-time field
// -- so a pointer to one IS a pointer to an Array, which is exactly the
// promise lhat_register_hostdata_subtype asks a host to make. godot.ArrayOfNode
// is therefore declared under godot.Array, and:
//
//   * a call handing over an ArrayOfNode where an Array is wanted is the
//     derived standing for its base, which is also Godot's own rule (a typed
//     array assigns to an untyped one outright)
//   * the other way round does not typecheck here, and would not have passed
//     Array::assign there either -- it is the direction the engine validates
//     element by element
//
// Which typed arrays exist is which the bound methods asked for, so the list
// is generated (lhat_godot_api.gen.cpp) rather than written here.

#ifndef LHAT_GODOT_CONTAINERS_H
#define LHAT_GODOT_CONTAINERS_H

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "lhat.h"

namespace godot {
namespace host {

struct Godot;

// One container type. `element` is written in 13 章's grammar and is what
// `at` answers; `base` is NULL for the two bare ones and "Array" for a typed
// array. `builtin` and `class_name` are what the engine calls the element,
// which is how a value coming back finds its tag again -- and what set_typed
// is handed when a fresh one is made.
struct BoundContainer {
    const char *name;        // "Array", "Dictionary", "ArrayOfNode"
    const char *element;     // "any^", "godot.Node"
    const char *base;        // NULL, or "Array"
    uint8_t builtin;         // Variant::Type of the element, NIL when untyped
    const char *class_name;  // NULL unless builtin is OBJECT
    // Filled by the first registration and the process's from then on, the
    // way a BoundMethod's is -- a maker is registered with its own row as
    // context and reads the module back out of it.
    const Godot *module;
};

// The typed arrays, generated from what the bound methods ask for.
extern BoundContainer typed_arrays[];
extern const size_t typed_array_count;

// Registers godot.Array, godot.Dictionary and every typed array under the
// first. Before any checking (05 の 8.7).
bool register_containers(LhatProgram *program, Godot *module);

// A value of godot.Array, tagged with the typed one its elements say it is
// and with the bare one otherwise. False when the machine ran out of memory.
bool make_array(LhatMachine *machine, const Godot *module, const Array &from,
                LhatValue *out);

// The same for godot.Dictionary, which has one type however its elements are
// declared -- the engine writes typeddictionary:: on two properties in the
// whole API and on no method at all.
bool make_dictionary(LhatMachine *machine, const Godot *module,
                     const Dictionary &from, LhatValue *out);

// The Array or Dictionary one of ours holds, or NULL when the value is not
// one. A typed array answers to the bare tag as well as its own (8.8改), so
// one call reaches every one of them.
Array *held_array(LhatValue value, const Godot *module);
Dictionary *held_dictionary(LhatValue value, const Godot *module);

// One element across the seam, as the container `what` says its elements
// are. Everything from_variant answers, and a handle rather than a table
// where the element is itself an Array or a Dictionary -- so what `at` gives
// back has the type the signature promised.
//
// 8.9's values answer as themselves ONLY where the signature names the type.
// A container whose element is any^ -- the bare Array, every Dictionary --
// cannot carry one at all: 8.9 bars a value from anything that outlives a
// frame, and a host has no way to box one (box^ is the spelling that makes
// one, and the heap is the machine's). Such an element answers nothing,
// which is what from_variant has always done with a Vector2.
LhatValue element_answer(LhatMachine *machine, const BoundContainer *what,
                         const Variant &held);

}  // namespace host
}  // namespace godot

#endif  // LHAT_GODOT_CONTAINERS_H
