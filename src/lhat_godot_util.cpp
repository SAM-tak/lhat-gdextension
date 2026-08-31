#include "lhat_godot_util.h"

#include <godot_cpp/core/math_defs.hpp>
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

// 02 の 14.8: one number^ over integers and reals, so which one arrives is
// what it was made as and either is read the same way here.
double number_at(const LhatValue *arguments, size_t count, size_t at)
{
    if (at >= count) {
        return 0.0;
    }
    return lhat_is_integer(arguments[at]) ? (double)lhat_as_integer(arguments[at])
                                          : lhat_as_real(arguments[at]);
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

// ---------------------------------------------------------------------------
// 78 of the 114 are mathematics, and every one of them is here rather than in
// std.math -- which this host registers none of. One reason settles it:
// std.math works in DEGREES and Godot works in radians, and the engine's own
// 139 angle-carrying methods cannot be converted, since extension_api.json
// says nothing about what a float means. Two conventions in one expression is
// worse than either, so the engine's is the one, and sin(x) here takes what
// set_rotation takes.
//
// The eleven that take or answer a Variant are left out: abs, min, max, clamp,
// sign, lerp, snapped, wrap, round, floor and ceil each have an f and an i of
// their own, which say outright what the Variant one would have decided by
// looking. 02 の 14.8 makes both number^ to a script either way.

template <double (*Fn)(double)>
void real_of_real(LhatMachine *machine, void *context,
             const LhatValue *arguments, size_t count,
             LhatValue *answers, int *answer_count)
{
    (void)machine;
    (void)context;
    answers[0] = lhat_real(Fn(number_at(arguments, count, 0)));
    *answer_count = 1;
}

template <double (*Fn)(double, double)>
void real_of_2(LhatMachine *machine, void *context,
          const LhatValue *arguments, size_t count,
          LhatValue *answers, int *answer_count)
{
    (void)machine;
    (void)context;
    answers[0] = lhat_real(Fn(number_at(arguments, count, 0), number_at(arguments, count, 1)));
    *answer_count = 1;
}

template <double (*Fn)(double, double, double)>
void real_of_3(LhatMachine *machine, void *context,
          const LhatValue *arguments, size_t count,
          LhatValue *answers, int *answer_count)
{
    (void)machine;
    (void)context;
    answers[0] = lhat_real(Fn(number_at(arguments, count, 0), number_at(arguments, count, 1), number_at(arguments, count, 2)));
    *answer_count = 1;
}

template <double (*Fn)(double, double, double, double, double)>
void real_of_5(LhatMachine *machine, void *context,
          const LhatValue *arguments, size_t count,
          LhatValue *answers, int *answer_count)
{
    (void)machine;
    (void)context;
    answers[0] = lhat_real(Fn(number_at(arguments, count, 0), number_at(arguments, count, 1), number_at(arguments, count, 2), number_at(arguments, count, 3), number_at(arguments, count, 4)));
    *answer_count = 1;
}

template <bool (*Fn)(double)>
void bool_of_real(LhatMachine *machine, void *context,
             const LhatValue *arguments, size_t count,
             LhatValue *answers, int *answer_count)
{
    (void)machine;
    (void)context;
    answers[0] = lhat_bool(Fn(number_at(arguments, count, 0)));
    *answer_count = 1;
}

template <int64_t (*Fn)(double)>
void whole_of_real(LhatMachine *machine, void *context,
              const LhatValue *arguments, size_t count,
              LhatValue *answers, int *answer_count)
{
    (void)machine;
    (void)context;
    answers[0] = lhat_integer(Fn(number_at(arguments, count, 0)));
    *answer_count = 1;
}

template <int64_t (*Fn)(int64_t)>
void whole_of_whole(LhatMachine *machine, void *context,
               const LhatValue *arguments, size_t count,
               LhatValue *answers, int *answer_count)
{
    (void)machine;
    (void)context;
    answers[0] = lhat_integer(Fn((int64_t)number_at(arguments, count, 0)));
    *answer_count = 1;
}

template <int64_t (*Fn)(int64_t, int64_t)>
void whole_of_2(LhatMachine *machine, void *context,
           const LhatValue *arguments, size_t count,
           LhatValue *answers, int *answer_count)
{
    (void)machine;
    (void)context;
    answers[0] = lhat_integer(Fn((int64_t)number_at(arguments, count, 0), (int64_t)number_at(arguments, count, 1)));
    *answer_count = 1;
}

template <double (*Fn)(double, double, double, double, double, double, double, double)>
void real_of_8(LhatMachine *machine, void *context,
          const LhatValue *arguments, size_t count,
          LhatValue *answers, int *answer_count)
{
    (void)machine;
    (void)context;
    answers[0] = lhat_real(Fn(number_at(arguments, count, 0), number_at(arguments, count, 1), number_at(arguments, count, 2), number_at(arguments, count, 3), number_at(arguments, count, 4), number_at(arguments, count, 5), number_at(arguments, count, 6), number_at(arguments, count, 7)));
    *answer_count = 1;
}

template <int64_t (*Fn)(int64_t, int64_t, int64_t)>
void whole_of_3(LhatMachine *machine, void *context,
           const LhatValue *arguments, size_t count,
           LhatValue *answers, int *answer_count)
{
    (void)machine;
    (void)context;
    answers[0] = lhat_integer(Fn((int64_t)number_at(arguments, count, 0), (int64_t)number_at(arguments, count, 1), (int64_t)number_at(arguments, count, 2)));
    *answer_count = 1;
}

template <bool (*Fn)(double, double)>
void bool_of_2(LhatMachine *machine, void *context,
          const LhatValue *arguments, size_t count,
          LhatValue *answers, int *answer_count)
{
    (void)machine;
    (void)context;
    answers[0] = lhat_bool(Fn(number_at(arguments, count, 0), number_at(arguments, count, 1)));
    *answer_count = 1;
}

template <int64_t (*Fn)(double, int64_t)>
void whole_of_real_whole(LhatMachine *machine, void *context,
                    const LhatValue *arguments, size_t count,
                    LhatValue *answers, int *answer_count)
{
    (void)machine;
    (void)context;
    answers[0] = lhat_integer(Fn(number_at(arguments, count, 0), (int64_t)number_at(arguments, count, 1)));
    *answer_count = 1;
}

// ---------------------------------------------------------------------------
// GDScript's PI, TAU, INF and NAN. Written as calls because a host has no way
// to register a constant -- every lhat_register_* takes a LhatHostFn, and
// std.math itself carries functions and no constants. A nullary f^ is the
// nearest thing, and it beats making every writer spell the digits out.

template <double (*Fn)()>
void util_constant(LhatMachine *machine, void *context,
                   const LhatValue *arguments, size_t count,
                   LhatValue *answers, int *answer_count)
{
    (void)machine;
    (void)context;
    (void)arguments;
    (void)count;
    answers[0] = lhat_real(Fn());
    *answer_count = 1;
}

double pi_value() { return Math_PI; }
double tau_value() { return Math_TAU; }
double inf_value() { return Math_INF; }
double nan_value() { return Math_NAN; }

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

        {"PI", "f^ -> number^;", util_constant<pi_value>},
        {"TAU", "f^ -> number^;", util_constant<tau_value>},
        {"INF", "f^ -> number^;", util_constant<inf_value>},
        {"NAN", "f^ -> number^;", util_constant<nan_value>},

        {"absf", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::absf>},
        {"acos", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::acos>},
        {"acosh", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::acosh>},
        {"asin", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::asin>},
        {"asinh", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::asinh>},
        {"atan", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::atan>},
        {"atanh", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::atanh>},
        {"ceilf", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::ceilf>},
        {"cos", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::cos>},
        {"cosh", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::cosh>},
        {"db_to_linear", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::db_to_linear>},
        {"deg_to_rad", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::deg_to_rad>},
        {"exp", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::exp>},
        {"floorf", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::floorf>},
        {"linear_to_db", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::linear_to_db>},
        {"log", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::log>},
        {"rad_to_deg", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::rad_to_deg>},
        {"roundf", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::roundf>},
        {"signf", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::signf>},
        {"sin", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::sin>},
        {"sinh", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::sinh>},
        {"sqrt", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::sqrt>},
        {"tan", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::tan>},
        {"tanh", "f^number^ -> number^;",
         real_of_real<UtilityFunctions::tanh>},
        {"angle_difference", "f^number^, number^ -> number^;",
         real_of_2<UtilityFunctions::angle_difference>},
        {"atan2", "f^number^, number^ -> number^;",
         real_of_2<UtilityFunctions::atan2>},
        {"ease", "f^number^, number^ -> number^;",
         real_of_2<UtilityFunctions::ease>},
        {"fmod", "f^number^, number^ -> number^;",
         real_of_2<UtilityFunctions::fmod>},
        {"fposmod", "f^number^, number^ -> number^;",
         real_of_2<UtilityFunctions::fposmod>},
        {"maxf", "f^number^, number^ -> number^;",
         real_of_2<UtilityFunctions::maxf>},
        {"minf", "f^number^, number^ -> number^;",
         real_of_2<UtilityFunctions::minf>},
        {"pingpong", "f^number^, number^ -> number^;",
         real_of_2<UtilityFunctions::pingpong>},
        {"pow", "f^number^, number^ -> number^;",
         real_of_2<UtilityFunctions::pow>},
        {"snappedf", "f^number^, number^ -> number^;",
         real_of_2<UtilityFunctions::snappedf>},
        {"clampf", "f^number^, number^, number^ -> number^;",
         real_of_3<UtilityFunctions::clampf>},
        {"inverse_lerp", "f^number^, number^, number^ -> number^;",
         real_of_3<UtilityFunctions::inverse_lerp>},
        {"lerp_angle", "f^number^, number^, number^ -> number^;",
         real_of_3<UtilityFunctions::lerp_angle>},
        {"lerpf", "f^number^, number^, number^ -> number^;",
         real_of_3<UtilityFunctions::lerpf>},
        {"move_toward", "f^number^, number^, number^ -> number^;",
         real_of_3<UtilityFunctions::move_toward>},
        {"rotate_toward", "f^number^, number^, number^ -> number^;",
         real_of_3<UtilityFunctions::rotate_toward>},
        {"smoothstep", "f^number^, number^, number^ -> number^;",
         real_of_3<UtilityFunctions::smoothstep>},
        {"wrapf", "f^number^, number^, number^ -> number^;",
         real_of_3<UtilityFunctions::wrapf>},
        {"bezier_derivative", "f^number^, number^, number^, number^, number^ -> number^;",
         real_of_5<UtilityFunctions::bezier_derivative>},
        {"bezier_interpolate", "f^number^, number^, number^, number^, number^ -> number^;",
         real_of_5<UtilityFunctions::bezier_interpolate>},
        {"cubic_interpolate", "f^number^, number^, number^, number^, number^ -> number^;",
         real_of_5<UtilityFunctions::cubic_interpolate>},
        {"cubic_interpolate_angle", "f^number^, number^, number^, number^, number^ -> number^;",
         real_of_5<UtilityFunctions::cubic_interpolate_angle>},
        {"remap", "f^number^, number^, number^, number^, number^ -> number^;",
         real_of_5<UtilityFunctions::remap>},
        {"is_finite", "f^number^ -> bool^;",
         bool_of_real<UtilityFunctions::is_finite>},
        {"is_inf", "f^number^ -> bool^;",
         bool_of_real<UtilityFunctions::is_inf>},
        {"is_nan", "f^number^ -> bool^;",
         bool_of_real<UtilityFunctions::is_nan>},
        {"is_zero_approx", "f^number^ -> bool^;",
         bool_of_real<UtilityFunctions::is_zero_approx>},
        {"ceili", "f^number^ -> number^;",
         whole_of_real<UtilityFunctions::ceili>},
        {"floori", "f^number^ -> number^;",
         whole_of_real<UtilityFunctions::floori>},
        {"roundi", "f^number^ -> number^;",
         whole_of_real<UtilityFunctions::roundi>},
        {"step_decimals", "f^number^ -> number^;",
         whole_of_real<UtilityFunctions::step_decimals>},
        {"absi", "f^number^ -> number^;",
         whole_of_whole<UtilityFunctions::absi>},
        {"nearest_po2", "f^number^ -> number^;",
         whole_of_whole<UtilityFunctions::nearest_po2>},
        {"signi", "f^number^ -> number^;",
         whole_of_whole<UtilityFunctions::signi>},
        {"maxi", "f^number^, number^ -> number^;",
         whole_of_2<UtilityFunctions::maxi>},
        {"mini", "f^number^, number^ -> number^;",
         whole_of_2<UtilityFunctions::mini>},
        {"posmod", "f^number^, number^ -> number^;",
         whole_of_2<UtilityFunctions::posmod>},
        {"cubic_interpolate_angle_in_time", "f^number^, number^, number^, number^, number^, number^, number^, number^ -> number^;",
         real_of_8<UtilityFunctions::cubic_interpolate_angle_in_time>},
        {"cubic_interpolate_in_time", "f^number^, number^, number^, number^, number^, number^, number^, number^ -> number^;",
         real_of_8<UtilityFunctions::cubic_interpolate_in_time>},
        {"clampi", "f^number^, number^, number^ -> number^;",
         whole_of_3<UtilityFunctions::clampi>},
        {"wrapi", "f^number^, number^, number^ -> number^;",
         whole_of_3<UtilityFunctions::wrapi>},
        {"is_equal_approx", "f^number^, number^ -> bool^;",
         bool_of_2<UtilityFunctions::is_equal_approx>},
        {"snappedi", "f^number^, number^ -> number^;",
         whole_of_real_whole<UtilityFunctions::snappedi>},
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
