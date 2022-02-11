#ifndef BUILTIN_H
#define BUILTIN_H

#include "laureimage.h"
#include "laurelang.h"
#include "predicates.h"

Instance builtin_integer();
/*
Instance builtin_char();
Instance builtin_string();
Instance builtin_index();
*/

struct Builtin {
    Instance (*generate)();
};

struct Builtin BUILTIN_INSTANCES[] = {
    {builtin_integer},
    /*
    {builtin_char},
    {builtin_string},
    {builtin_index},
    */
};

const struct BuiltinPred BUILTIN_PREDICATES[] = {
    {"__+", laure_predicate_integer_plus, 2, "builtin addition predicate", {"x:int y:int", "int"}, false},
    {"__>", laure_constraint_gt, 2, "builtin greater than predicate", {"x:int y:int", NULL}, true},
    {"__gte", laure_constraint_gte, 2, "builtin greater than/equal predicate", {"x:int y:int", NULL}, true},
    {"__*", laure_predicate_integer_multiply, 2, "builtin multiplication predicate", {"x:int y:int", "int"}, false}
    /*
    {"__message", laure_predicate_message, 1, "Builtin predicate to operate with IO with strings", {"s:string", NULL}, false},
    {"__getchar", laure_predicate_getchar, 0, "Builtin predicate to operate with IO with chars", {"", "char"}, false},
    {"__*", laure_predicate_integer_mutiple, 2, "Builtin multiple predicate", {"x:int y:int", "int"}, false},
    {"__>", laure_predicate_gt, 2, "", {"x:int y:int", NULL}, true},
    {"__>eq", laure_predicate_gte, 2, "", {"x:int y:int", NULL}, true},
    {"repr", laure_predicate_repr, 1, "", {"!:_", "string"}, false},
    {"format", laure_predicate_format, 1, "", {"s:string", "string"}, false},
    {"length", laure_predicate_length, 1, "", {"x:_", "index"}, false},
    */
};

void laure_register_builtins(laure_session_t*);

Instance *laure_cle_add_predicate(
    laure_session_t *session,
    string name,
    qresp (*pred)(preddata*, control_ctx*),
    int argc,
    string args_hint,
    string response_hint,
    bool is_constraint,
    string doc
);

#endif