#include "lhat_godot_handles.h"

#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/callable_custom.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <unordered_set>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "lhat_godot_module.h"
#include "lhat_host.h"
#include "lhat_variant.h"

namespace godot {
namespace host {
namespace {

const Godot *module_of(void *context)
{
    return (const Godot *)context;
}

bool name_of(LhatValue value, String *out)
{
    if (!lhat_is_object_kind(value, LHAT_OBJECT_STRING)) {
        return false;
    }
    const LhatString *text = (const LhatString *)lhat_as_object(value);
    *out = String::utf8(text->text, (int)text->length);
    return true;
}

LhatValue text_answer(LhatMachine *machine, const String &text)
{
    CharString bytes = text.utf8();
    LhatValue out = lhat_nil();
    return lhat_machine_make_string(machine, bytes.get_data(),
                                    (size_t)bytes.length(), &out)
               ? out
               : lhat_nil();
}

// ---------------------------------------------------------------------------
// Callable

class DeferredCall;

std::unordered_set<DeferredCall *> &pending_calls()
{
    static std::unordered_set<DeferredCall *> calls;
    return calls;
}

// Godot's queue owns the Callable; the machine's host root owns its closure.
// Cancellation leaves a harmless queued Callable after its machine is gone.
class DeferredCall : public CallableCustom {
    mutable LhatMachine *machine;
    LhatValue ticket;

    static bool equal(const CallableCustom *a, const CallableCustom *b) { return a == b; }
    static bool less(const CallableCustom *a, const CallableCustom *b)
    {
        return (uintptr_t)a < (uintptr_t)b;
    }

public:
    DeferredCall(LhatMachine *on, LhatValue held) : machine(on), ticket(held)
    {
        pending_calls().insert(this);
    }
    ~DeferredCall() override
    {
        Alone alone;
        release();
    }
    bool belongs_to(LhatMachine *on) const { return machine == on; }
    void release() const
    {
        if (machine != nullptr) {
            bool refused = false;
            lhat_machine_table_set(machine, lhat_machine_host_root(machine),
                                   ticket, lhat_nil(), &refused);
            machine = nullptr;
        }
        pending_calls().erase(const_cast<DeferredCall *>(this));
    }
    ObjectID get_object() const override { return ObjectID(); }
    // This Callable has no engine Object; cancellation makes it a valid no-op.
    bool is_valid() const override { return true; }
    uint32_t hash() const override { return (uint32_t)((uintptr_t)this >> 3); }
    String get_as_text() const override { return "godot.defer"; }
    CompareEqualFunc get_compare_equal_func() const override { return equal; }
    CompareLessFunc get_compare_less_func() const override { return less; }
    int get_argument_count(bool &valid) const override { valid = true; return 0; }

    void call(const Variant **arguments, int count, Variant &answer,
              GDExtensionCallError &error) const override
    {
        (void)arguments;
        (void)count;
        Alone alone;
        answer = Variant();
        error.error = GDEXTENSION_CALL_OK;
        if (machine == nullptr) {
            return;
        }
        LhatValue closure = lhat_table_get((LhatTable *)lhat_as_object(ticket),
                                           lhat_integer(0));
        LhatRunResult ran = lhat_machine_call(machine, closure, nullptr, 0);
        if (ran.status != LHAT_RUN_OK) {
            UtilityFunctions::push_error(run_problem(machine, "godot.defer", ran));
        } else if (lhat_is_object_kind(ran.value, LHAT_OBJECT_COROUTINE)) {
            UtilityFunctions::push_error("godot.defer: the deferred procedure must not yield");
        }
        release();
    }
};

void defer_procedure(LhatMachine *machine, void *context,
                     const LhatValue *arguments, size_t count,
                     LhatValue *answers, int *answer_count)
{
    (void)context;
    (void)answers;
    *answer_count = 0;
    Alone alone;
    if (count != 1) {
        return;
    }
    LhatValue ticket = lhat_nil();
    bool refused = false;
    if (!lhat_machine_make_table(machine, &ticket) ||
        !lhat_machine_table_set(machine, (LhatTable *)lhat_as_object(ticket),
                                lhat_integer(0), arguments[0], &refused) || refused ||
        !lhat_machine_table_set(machine, lhat_machine_host_root(machine),
                                ticket, lhat_bool(true), &refused) || refused) {
        lhat_machine_panic_text(machine, "godot.defer: out of memory retaining the procedure");
        return;
    }
    Callable(memnew(DeferredCall(machine, ticket))).call_deferred();
}

const Callable *held_callable(LhatValue value, const Godot *module)
{
    return (const Callable *)lhat_hostdata_pointer(value,
                                                   module->callable_tag);
}

void callable_is_valid(LhatMachine *machine, void *context,
                       const LhatValue *arguments, size_t count,
                       LhatValue *answers, int *answer_count)
{
    (void)machine;
    const Callable *held =
        count > 0 ? held_callable(arguments[0], module_of(context)) : nullptr;
    answers[0] = lhat_bool(held != nullptr && held->is_valid());
    *answer_count = 1;
}

void callable_method(LhatMachine *machine, void *context,
                     const LhatValue *arguments, size_t count,
                     LhatValue *answers, int *answer_count)
{
    const Callable *held =
        count > 0 ? held_callable(arguments[0], module_of(context)) : nullptr;
    answers[0] = text_answer(machine, held != nullptr ? String(held->get_method())
                                                : String());
    *answer_count = 1;
}

void callable_object(LhatMachine *machine, void *context,
                     const LhatValue *arguments, size_t count,
                     LhatValue *answers, int *answer_count)
{
    const Godot *module = module_of(context);
    const Callable *held = count > 0 ? held_callable(arguments[0], module)
                                     : nullptr;
    LhatValue out = lhat_nil();
    answers[0] = make_object(machine, module,
                       held != nullptr ? held->get_object() : nullptr, &out)
               ? out
               : lhat_nil();
    *answer_count = 1;
}

// 12.6's variadic tail: what a call writes after the receiver is what the
// Callable is handed.
void callable_call(LhatMachine *machine, void *context,
                   const LhatValue *arguments, size_t count,
                   LhatValue *answers, int *answer_count)
{
    const Godot *module = module_of(context);
    const Callable *held = count > 0 ? held_callable(arguments[0], module)
                                     : nullptr;
    if (held == nullptr) {
        return;
    }
    Array passed;
    for (size_t i = 1; i < count; i++) {
        passed.push_back(to_variant(arguments[i], module));
    }
    LhatValue out = lhat_nil();
    answers[0] = from_variant(machine, held->callv(passed), &out, module) ? out
                                                                   : lhat_nil();
    *answer_count = 1;
}

void callable_dispose(LhatMachine *machine, void *context,
                      const LhatValue *arguments, size_t count,
                      LhatValue *answers, int *answer_count)
{
    (void)machine;
    if (count == 0) {
        return;
    }
    Callable *held = (Callable *)lhat_hostdata_pointer(
        arguments[0], module_of(context)->callable_tag);
    if (held != nullptr) {
        memdelete(held);
    }
}

// f^godot.Object, string^-> godot.Callable
void make_callable_of(LhatMachine *machine, void *context,
                      const LhatValue *arguments, size_t count,
                      LhatValue *answers, int *answer_count)
{
    const Godot *module = module_of(context);
    String named;
    Object *object = count > 0 ? object_of(arguments[0], module) : nullptr;
    if (count < 2 || !name_of(arguments[1], &named)) {
        return;
    }
    LhatValue out = lhat_nil();
    answers[0] = make_callable(machine, module, Callable(object, StringName(named)),
                         &out)
               ? out
               : lhat_nil();
    *answer_count = 1;
}

// ---------------------------------------------------------------------------
// Signal

const Signal *held_signal(LhatValue value, const Godot *module)
{
    return (const Signal *)lhat_hostdata_pointer(value, module->signal_tag);
}

void signal_name(LhatMachine *machine, void *context,
                 const LhatValue *arguments, size_t count,
                 LhatValue *answers, int *answer_count)
{
    const Signal *held =
        count > 0 ? held_signal(arguments[0], module_of(context)) : nullptr;
    answers[0] = text_answer(machine,
                       held != nullptr ? String(held->get_name()) : String());
    *answer_count = 1;
}

void signal_object(LhatMachine *machine, void *context,
                   const LhatValue *arguments, size_t count,
                   LhatValue *answers, int *answer_count)
{
    const Godot *module = module_of(context);
    const Signal *held = count > 0 ? held_signal(arguments[0], module)
                                   : nullptr;
    LhatValue out = lhat_nil();
    answers[0] = make_object(machine, module,
                       held != nullptr ? held->get_object() : nullptr, &out)
               ? out
               : lhat_nil();
    *answer_count = 1;
}

void signal_is_null(LhatMachine *machine, void *context,
                    const LhatValue *arguments, size_t count,
                    LhatValue *answers, int *answer_count)
{
    (void)machine;
    const Signal *held =
        count > 0 ? held_signal(arguments[0], module_of(context)) : nullptr;
    answers[0] = lhat_bool(held == nullptr || held->is_null());
    *answer_count = 1;
}

void signal_connect(LhatMachine *machine, void *context,
                    const LhatValue *arguments, size_t count,
                    LhatValue *answers, int *answer_count)
{
    (void)machine;
    const Godot *module = module_of(context);
    const Signal *held = count > 0 ? held_signal(arguments[0], module)
                                   : nullptr;
    const Callable *to = count > 1 ? held_callable(arguments[1], module)
                                   : nullptr;
    if (held != nullptr && to != nullptr) {
        const_cast<Signal *>(held)->connect(*to);
    }
}

void signal_disconnect(LhatMachine *machine, void *context,
                       const LhatValue *arguments, size_t count,
                       LhatValue *answers, int *answer_count)
{
    (void)machine;
    const Godot *module = module_of(context);
    const Signal *held = count > 0 ? held_signal(arguments[0], module)
                                   : nullptr;
    const Callable *from = count > 1 ? held_callable(arguments[1], module)
                                     : nullptr;
    if (held != nullptr && from != nullptr) {
        const_cast<Signal *>(held)->disconnect(*from);
    }
}

void signal_is_connected(LhatMachine *machine, void *context,
                         const LhatValue *arguments, size_t count,
                         LhatValue *answers, int *answer_count)
{
    (void)machine;
    const Godot *module = module_of(context);
    const Signal *held = count > 0 ? held_signal(arguments[0], module)
                                   : nullptr;
    const Callable *to = count > 1 ? held_callable(arguments[1], module)
                                   : nullptr;
    answers[0] = lhat_bool(held != nullptr && to != nullptr &&
                     held->is_connected(*to));
    *answer_count = 1;
}

// A signal is emitted through the object that carries it -- Signal::emit is a
// template over C++ arguments and has no run-time arity, and emit_signal
// takes the name and the rest.
void signal_emit(LhatMachine *machine, void *context,
                 const LhatValue *arguments, size_t count,
                 LhatValue *answers, int *answer_count)
{
    (void)machine;
    const Godot *module = module_of(context);
    const Signal *held = count > 0 ? held_signal(arguments[0], module)
                                   : nullptr;
    Object *object = held != nullptr ? held->get_object() : nullptr;
    if (object == nullptr) {
        return;
    }
    Array passed;
    passed.push_back(held->get_name());
    for (size_t i = 1; i < count; i++) {
        passed.push_back(to_variant(arguments[i], module));
    }
    object->callv("emit_signal", passed);
}

void signal_dispose(LhatMachine *machine, void *context,
                    const LhatValue *arguments, size_t count,
                    LhatValue *answers, int *answer_count)
{
    (void)machine;
    if (count == 0) {
        return;
    }
    Signal *held = (Signal *)lhat_hostdata_pointer(
        arguments[0], module_of(context)->signal_tag);
    if (held != nullptr) {
        memdelete(held);
    }
}

struct KnownSignal {
    const char *type;
    const char *name;
    const Godot *module;
};

// The declaration's class owns the accessor; derived classes inherit it.
// These descriptors outlive the callbacks registered on every program.
KnownSignal known_signals[] = {
    {"Timer", "timeout", nullptr},
    {"SceneTreeTimer", "timeout", nullptr},
    {"SceneTree", "process_frame", nullptr},
    {"SceneTree", "physics_frame", nullptr},
    {"BaseButton", "pressed", nullptr},
    {"BaseButton", "toggled", nullptr},
    {"Tween", "finished", nullptr},
    {"Node", "ready", nullptr},
    {"Node", "tree_exited", nullptr},
    {"Area2D", "body_entered", nullptr},
    {"AnimationMixer", "animation_finished", nullptr},
};

// f^self^ -> godot.Signal: no signal name or emitted arguments are supplied
// by the caller. This returns a signal, without emitting it.
void known_signal(LhatMachine *machine, void *context,
                  const LhatValue *arguments, size_t count,
                  LhatValue *answers, int *answer_count)
{
    const KnownSignal *binding = (const KnownSignal *)context;
    const Godot *module = binding->module;
    Object *object = count > 0 ? object_of(arguments[0], module) : nullptr;
    if (object == nullptr) {
        return;
    }
    LhatValue out = lhat_nil();
    answers[0] = make_signal(machine, module, Signal(object, StringName(binding->name)),
                             &out)
                     ? out
                     : lhat_nil();
    *answer_count = 1;
}

// f^godot.Object, string^-> godot.Signal
void make_signal_of(LhatMachine *machine, void *context,
                    const LhatValue *arguments, size_t count,
                    LhatValue *answers, int *answer_count)
{
    const Godot *module = module_of(context);
    String named;
    Object *object = count > 0 ? object_of(arguments[0], module) : nullptr;
    if (object == nullptr || count < 2 || !name_of(arguments[1], &named)) {
        return;
    }
    LhatValue out = lhat_nil();
    answers[0] = make_signal(machine, module, Signal(object, StringName(named)),
                       &out)
               ? out
               : lhat_nil();
    *answer_count = 1;
}

}  // namespace

void cancel_deferred(LhatMachine *machine)
{
    Alone alone;
    auto &calls = pending_calls();
    for (auto at = calls.begin(); at != calls.end();) {
        DeferredCall *call = *at++;
        if (call->belongs_to(machine)) {
            call->release();
        }
    }
}

bool make_callable(LhatMachine *machine, const Godot *module,
                   const Callable &callable, LhatValue *out)
{
    if (module == nullptr) {
        return false;
    }
    Callable *held = memnew(Callable(callable));
    if (!lhat_machine_make_hostdata(machine, module->callable_tag, held, out)) {
        memdelete(held);
        return false;
    }
    return true;
}

bool make_signal(LhatMachine *machine, const Godot *module,
                 const Signal &signal, LhatValue *out)
{
    if (module == nullptr) {
        return false;
    }
    Signal *held = memnew(Signal(signal));
    if (!lhat_machine_make_hostdata(machine, module->signal_tag, held, out)) {
        memdelete(held);
        return false;
    }
    return true;
}

const Callable *callable_of(LhatValue value, const Godot *module)
{
    return module != nullptr ? held_callable(value, module) : nullptr;
}

const Signal *signal_of(LhatValue value, const Godot *module)
{
    return module != nullptr ? held_signal(value, module) : nullptr;
}

bool register_handles(LhatProgram *program, Godot *module)
{
    if (program == nullptr || module == nullptr) {
        return false;
    }
    module->callable_tag =
        lhat_register_hostdata_type(program, "godot", "Callable");
    module->signal_tag =
        lhat_register_hostdata_type(program, "godot", "Signal");
    if (module->callable_tag == nullptr || module->signal_tag == nullptr) {
        return false;
    }

    struct {
        const char *type;
        const char *name;
        const char *signature;
        LhatHostFn call;
    } members[] = {
        {"Callable", "isValid", "f^self^-> bool^;", callable_is_valid},
        {"Callable", "getMethod", "f^self^-> string^;", callable_method},
        {"Callable", "getObject", "f^self^-> godot.Object;", callable_object},
        {"Callable", "call", "f^self^, ... -> any^;", callable_call},
        {"Callable", "dispose", "p^self^;", callable_dispose},

        {"Signal", "getName", "f^self^-> string^;", signal_name},
        {"Signal", "getObject", "f^self^-> godot.Object;", signal_object},
        {"Signal", "isNull", "f^self^-> bool^;", signal_is_null},
        {"Signal", "connect", "p^self^, godot.Callable;", signal_connect},
        {"Signal", "disconnect", "p^self^, godot.Callable;",
         signal_disconnect},
        {"Signal", "isConnected", "f^self^, godot.Callable -> bool^;",
         signal_is_connected},
        {"Signal", "emit", "p^self^, ...;", signal_emit},
        {"Signal", "dispose", "p^self^;", signal_dispose},
    };
    for (const auto &member : members) {
        if (!lhat_register_member(program, "godot", member.type, member.name,
                                  member.signature, member.call, module)) {
            return false;
        }
    }

    for (auto &binding : known_signals) {
        if (binding.module == nullptr) {
            binding.module = module;
        }
        if (!lhat_register_member(program, "godot", binding.type, binding.name,
                                  "f^self^ -> godot.Signal;", known_signal,
                                  &binding)) {
            return false;
        }
    }

    // 14.15 with 8.8: neither is made out of nothing. A Callable names a
    // method on an object and a Signal one the object emits, so both are
    // made from an object and a name. The known signal accessors above
    // supply the signal name themselves.
    return lhat_register_func(program, "godot", "defer", "p^p^;;",
                              defer_procedure, module) &&
           lhat_register_func(program, "godot", "callable",
                              "f^godot.Object, string^-> godot.Callable;",
                              make_callable_of, module) &&
           lhat_register_func(program, "godot", "signal",
                              "f^godot.Object, string^-> godot.Signal;",
                              make_signal_of, module);
}

}  // namespace host
}  // namespace godot
