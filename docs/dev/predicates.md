# predicates c

## some conversions

all arguments are renamed to make them all pure vars. then they are imaged to their original expressions. renaming patterns are:

* `${n}` - where `{n}` is argument index, for arguments
* `$C` - for container-argument
* `$R` - for response

name conversion is instructions in the start of the predicate body (optimization):

```
$0 % X;
$1 % Y;
$R % Z;
```

predicates may be 3 types:

* header - declaring types `: ?primer(int) -> int.`
* rel - have no visible body (like a simple RELation) `?primer(5) -> 9`
* bodied - have a visible body `?primer(x) -> y { y = x + 4 }`

rel predicates are later constructed to bodied (having body of any length). Their values are converted to assertion expressions.

## predicate function

predicate function gets preddata which is:

```
preddata {
    int argc;
    struct predicate_arg *argv;
    Instance* resp;
    laure_stack_t *stack;
}
```

`pd_get_arg` is used to get arg instances from preddata:

```
Instance *a = pd_get_arg(pd, 0);
Instance *b = pd_get_arg(pd, 1);
// to get response:
Instance *c = pd->resp;
```

also predicate function receives control_ctx

predicate function may ideologically return boolean, but because of some reasons they may also return other states;

all possible states:

* `q_false` (0)
* `q_true` (1)
* `q_yield` (2) - predicate may share control to other processor, parent evaluator stops taking control
* `q_error` (3) - error may occur in predicate
* `q_stop` (4) - forcely stops iterating over choicepoint
* `q_continue` (5) - continues iteration over choicepoint
* `q_true_s` (6) - true but silent, needed for interactive interface

to return state `respond(qresp_state state, string error)` is used, error is `NULL` unless state is error.