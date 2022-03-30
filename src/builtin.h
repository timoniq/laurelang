#ifndef BUILTIN_H
#define BUILTIN_H

#include "laureimage.h"
#include "laurelang.h"
#include "predicates.h"

Instance builtin_integer();
Instance builtin_char();
Instance builtin_string();

struct Builtin {
    Instance (*generate)();
    string doc;
};

struct Builtin BUILTIN_INSTANCES[] = {
    {builtin_integer, "integer"},
    //{builtin_char, "unicode char"},
    //{builtin_string, "set of chars"}
};

const struct BuiltinPred BUILTIN_PREDICATES[] = {
    {"__+", laure_predicate_integer_plus, 2, "builtin addition predicate", {"x:int y:int", "int"}, false},
    {"__>", laure_constraint_gt, 2, "builtin greater than predicate", {"x:int y:int", NULL}, true},
    //{"__gte", laure_constraint_gte, 2, "builtin greater than/equal predicate", {"x:int y:int", NULL}, true},
    //{"__*", laure_predicate_integer_multiply, 2, "builtin multiplication predicate", {"x:int y:int", "int"}, false},
    //{"__message", laure_predicate_message, 1, "builtin message predicate", {"m:string", NULL}, false},
    //{"__sqrt", laure_predicate_sqrt, 1, "builtin sqrt predicate", {"x:int", "int"}, false}
};

void laure_register_builtins(laure_session_t*);

#endif
