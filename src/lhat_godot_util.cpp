#include "lhat_godot_util.h"

#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "lhat_godot_module.h"
#include "lhat_godot_packed.h"
#include "lhat_host.h"
#include "lhat_variant.h"

namespace godot {
namespace host {
namespace {

const Godot *module_of(void *context)
{
    return (const Godot *)context;
}

// The arguments joined the way print joins them, which is what the engine's
// own variadic ones do with theirs.
String said(const LhatValue *arguments, size_t count)
{
    String line;
    for (size_t i = 0; i < count; i++) {
        if (i > 0) {
            line += " ";
        }
        line += text_of(arguments[i]);
    }
    return line;
}

// ---------------------------------------------------------------------------
// 04 の 11.6改 has its own way of saying a run went wrong, and this is not
// that: these two put a line in the editor's own output, where a script's
// complaint sits beside the engine's. Written by hand because both take a
// variadic tail, which has no ptrcall and so no road through the generator.

void util_push_error(LhatMachine *machine, void *context,
                     const LhatValue *arguments, size_t count,
                     LhatValue *answers, int *answer_count)
{
    (void)machine;
    (void)context;
    UtilityFunctions::push_error(said(arguments, count));
}

void util_push_warning(LhatMachine *machine, void *context,
                       const LhatValue *arguments, size_t count,
                       LhatValue *answers, int *answer_count)
{
    (void)machine;
    (void)context;
    UtilityFunctions::push_warning(said(arguments, count));
}

// ---------------------------------------------------------------------------
// What a value is worth keeping as: text a person can read, and bytes a
// socket can carry. Both directions, since neither is worth having alone.

void util_var_to_str(LhatMachine *machine, void *context,
                     const LhatValue *arguments, size_t count,
                     LhatValue *answers, int *answer_count)
{
    if (count == 0) {
        return;
    }
    String text = UtilityFunctions::var_to_str(
        to_variant(arguments[0], module_of(context)));
    CharString bytes = text.utf8();
    LhatValue out = lhat_nil();
    if (!lhat_machine_make_string(machine, bytes.get_data(),
                                  (size_t)bytes.length(), &out)) {
        return;
    }
    answers[0] = out;
    *answer_count = 1;
}

void util_str_to_var(LhatMachine *machine, void *context,
                     const LhatValue *arguments, size_t count,
                     LhatValue *answers, int *answer_count)
{
    if (count == 0) {
        return;
    }
    Variant held = UtilityFunctions::str_to_var(text_of(arguments[0]));
    LhatValue out = lhat_nil();
    if (!from_variant(machine, held, &out, module_of(context))) {
        return;
    }
    answers[0] = out;
    *answer_count = 1;
}

void util_var_to_bytes(LhatMachine *machine, void *context,
                       const LhatValue *arguments, size_t count,
                       LhatValue *answers, int *answer_count)
{
    const Godot *module = module_of(context);
    if (count == 0) {
        return;
    }
    PackedByteArray made =
        UtilityFunctions::var_to_bytes(to_variant(arguments[0], module));
    LhatValue out = lhat_nil();
    if (!make_packed(machine, module, made, &out)) {
        return;
    }
    answers[0] = out;
    *answer_count = 1;
}

void util_bytes_to_var(LhatMachine *machine, void *context,
                       const LhatValue *arguments, size_t count,
                       LhatValue *answers, int *answer_count)
{
    const Godot *module = module_of(context);
    bool packed = false;
    Variant given =
        count > 0 ? packed_variant(arguments[0], module, &packed) : Variant();
    if (!packed) {
        return;
    }
    Variant held = UtilityFunctions::bytes_to_var((PackedByteArray)given);
    LhatValue out = lhat_nil();
    if (!from_variant(machine, held, &out, module)) {
        return;
    }
    answers[0] = out;
    *answer_count = 1;
}

// ---------------------------------------------------------------------------
// The engine's random, which is one stream the whole project shares -- a
// scene and a script and the engine itself all draw from it, so seeding it
// is what makes a run repeat. std.random is L^'s own and has no part in this
// one; which of the two a program wants is which of the two it draws from.

double number_at(const LhatValue *arguments, size_t count, size_t at)
{
    if (at >= count) {
        return 0.0;
    }
    return lhat_is_integer(arguments[at]) ? (double)lhat_as_integer(arguments[at])
                                          : lhat_as_real(arguments[at]);
}

void util_randomize(LhatMachine *machine, void *context,
                    const LhatValue *arguments, size_t count,
                    LhatValue *answers, int *answer_count)
{
    (void)machine;
    (void)context;
    (void)arguments;
    (void)count;
    UtilityFunctions::randomize();
}

void util_seed(LhatMachine *machine, void *context, const LhatValue *arguments,
               size_t count, LhatValue *answers, int *answer_count)
{
    (void)machine;
    (void)context;
    UtilityFunctions::seed((int64_t)number_at(arguments, count, 0));
}

void util_randi(LhatMachine *machine, void *context,
                const LhatValue *arguments, size_t count, LhatValue *answers,
                int *answer_count)
{
    (void)machine;
    (void)context;
    (void)arguments;
    (void)count;
    answers[0] = lhat_integer(UtilityFunctions::randi());
    *answer_count = 1;
}

void util_randf(LhatMachine *machine, void *context,
                const LhatValue *arguments, size_t count, LhatValue *answers,
                int *answer_count)
{
    (void)machine;
    (void)context;
    (void)arguments;
    (void)count;
    answers[0] = lhat_real(UtilityFunctions::randf());
    *answer_count = 1;
}

void util_randi_range(LhatMachine *machine, void *context,
                      const LhatValue *arguments, size_t count,
                      LhatValue *answers, int *answer_count)
{
    (void)machine;
    (void)context;
    answers[0] = lhat_integer(
        UtilityFunctions::randi_range((int64_t)number_at(arguments, count, 0),
                                      (int64_t)number_at(arguments, count, 1)));
    *answer_count = 1;
}

void util_randf_range(LhatMachine *machine, void *context,
                      const LhatValue *arguments, size_t count,
                      LhatValue *answers, int *answer_count)
{
    (void)machine;
    (void)context;
    answers[0] = lhat_real(UtilityFunctions::randf_range(
        number_at(arguments, count, 0), number_at(arguments, count, 1)));
    *answer_count = 1;
}

void util_randfn(LhatMachine *machine, void *context,
                 const LhatValue *arguments, size_t count, LhatValue *answers,
                 int *answer_count)
{
    (void)machine;
    (void)context;
    answers[0] = lhat_real(UtilityFunctions::randfn(
        number_at(arguments, count, 0), number_at(arguments, count, 1)));
    *answer_count = 1;
}

// The one that does not touch the shared stream: a seed in, and the number
// drawn from it beside the seed to draw the next one with.
void util_rand_from_seed(LhatMachine *machine, void *context,
                         const LhatValue *arguments, size_t count,
                         LhatValue *answers, int *answer_count)
{
    const Godot *module = module_of(context);
    PackedInt64Array made = UtilityFunctions::rand_from_seed(
        (int64_t)number_at(arguments, count, 0));
    LhatValue out = lhat_nil();
    if (!make_packed(machine, module, made, &out)) {
        return;
    }
    answers[0] = out;
    *answer_count = 1;
}

}  // namespace

bool register_util(LhatProgram *program, Godot *module)
{
    if (program == nullptr || module == nullptr) {
        return false;
    }
    // The engine's own spelling, as a singleton's methods keep theirs: these
    // are the same functions GDScript writes, and a script reading both
    // should not have to hold two names for one thing.
    const struct {
        const char *name;
        const char *signature;
        LhatHostFn call;
    } every[] = {
        {"push_error", "p^...;", util_push_error},
        {"push_warning", "p^...;", util_push_warning},

        {"var_to_str", "f^any^ -> string^;", util_var_to_str},
        {"str_to_var", "f^string^ -> any^;", util_str_to_var},
        {"var_to_bytes", "f^any^ -> godot.PackedByteArray;",
         util_var_to_bytes},
        {"bytes_to_var", "f^godot.PackedByteArray -> any^;",
         util_bytes_to_var},

        {"randomize", "p^;", util_randomize},
        {"seed", "p^number^;", util_seed},
        {"randi", "f^ -> number^;", util_randi},
        {"randf", "f^ -> number^;", util_randf},
        {"randi_range", "f^number^, number^ -> number^;", util_randi_range},
        {"randf_range", "f^number^, number^ -> number^;", util_randf_range},
        {"randfn", "f^number^, number^ -> number^;", util_randfn},
        {"rand_from_seed", "f^number^ -> godot.PackedInt64Array;",
         util_rand_from_seed},
    };
    for (const auto &one : every) {
        if (!lhat_register_func(program, "godot", one.name, one.signature,
                                one.call, module)) {
            return false;
        }
    }
    return true;
}

}  // namespace host
}  // namespace godot
