#ifndef BUILTIN_H
#define BUILTIN_H

#include "laureimage.h"
#include "laurelang.h"
#include "standard.h"

Instance builtin_integer();
Instance builtin_char();
Instance builtin_string();
Instance builtin_formatting();

struct Builtin {
    Instance (*generate)();
    string doc;
};

struct Builtin BUILTIN_INSTANCES[] = {
    {builtin_integer, "integer"},
    {builtin_char, "unicode char"},
    {builtin_string, "set of chars"},
    {builtin_formatting, "`string` formatting"}
};

const laure_builtin_predicate BUILTIN_PREDICATES[] = {
    {"__+", laure_predicate_integer_plus, 2, "builtin addition predicate", {"x:int y:int", "int"}, false},
    {"__>", laure_constraint_gt, 2, "builtin greater than predicate", {"x:int y:int", NULL}, true},
    {"__gte", laure_constraint_gte, 2, "builtin greater than/equal predicate", {"x:int y:int", NULL}, true},
    {"__*", laure_predicate_integer_multiply, 2, "builtin multiplication predicate", {"x:int y:int", "int"}, false},
    {"__*mod", laure_predicate_integer_multiply, 3, "builtin multiplication predicate with mod", {"x:int y:int r:int", "int"}, false},
    {"__message", laure_predicate_message, 1, "builtin message predicate", {"m:string", NULL}, false},
    {"__sqrt", laure_predicate_sqrt, 1, "builtin sqrt predicate", {"x:int", "int"}, false},
    {"__sqrt_round", laure_predicate_sqrt, 2, "builtin sqrt predicate with rounding", {"x:int r:Rounding", "int"}, false},
    {"repr", laure_predicate_repr, 1, "builtin representation predicate", {"ins:_", "string"}, false},
    {"__format", laure_predicate_format, 1, "builtin string formatting", {"f:formatting", "string"}, false},
    {"__bag", laure_predicate_bag, 2, "builtin bag", {"from:_ to:_"}, false},
    {"__bag_sz", laure_predicate_bag, 3, "builtin bag sized", {"from:_ to:_ size:int"}, false},
    {"each", laure_predicate_each, 1, "builtin array each predicate", {"arr:_", "_"}, false},
    {"__by_idx", laure_predicate_by_idx, 2, "builtin array by idx predicate", {"arr:_ idx:int", "_"}, false},
    {"length", laure_predicate_length, 1, "builtin array length predicate", {"arr:_", "int"}, false},
    {"__append", laure_predicate_append, 2, "builtin array append predicate", {"to:_ with:_", "_"}, false},
    {"__map", laure_predicate_map, 2, "builtin map", {"arr:_ mapping:_", "_"}, false},
    {"union", laure_contraint_union, 2, "union constraint", {"t1:_ t2:_", "_"}, true},
};

void laure_register_builtins(laure_session_t*);

laure_enum_atom ROUNDING_ATOMU[] = {
    {"up", RoundingUp},
    {"down", RoundingDown}
};

size_t rounding_atomu_size();

#endif
