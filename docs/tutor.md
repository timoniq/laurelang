# Instance

Laurelang stack consists of instances. Each instance must have a state (unified/instantiated or not unified/instantiated). Instantiated instances have a single concrete value specified. Not instantiated instances have typically a domain.

Let's look at `int` for instance. As an example, in instantiated state it may be 42, in not instantiated state: (0, 100] domain. Instances may be ancestors of other instances. Ancestor creation is called imaging. There is imaging operation in laurelang symbolised with `~`, so if we want to create an instance imaged from int (which is just a infinite integer domain in fact), we use:

```laurelang
a ~ int
```

Now a have the same domain as int and int is ancestor of `a`, so if we run this query again we will get true. Also we will get true if we run `a ~ a`, `int ~ int`. Some instances like `int`, `char`, `string` are locked and have a special privilege in logical operations

Instances may also be asserted. Assertion may be treated as a predicate (but not really). We declare that the left side is equal to the right side, and elements are trying to get instantiated. Note:

* Builtin and artificially locked instances are not getting instantiated
* Left element have a previlege for instantiation

```laurelang
a = int
```

This query means that a is equal to some int. So if we run it we will be trying to iterate on infinity which is not possible in standard way (so will get error normally), but we may use some progression to iterate like this: `0 -> 1 -> -1 -> 2 -> -2 -> ...`. To use this generator for int you may set laurelang flag `-int_jmp`.

## Instance components

Instance consists of name, image, representation generator, docstring and translator.  
Image can be derived from these types: integer, array, structure, atom and predicate of two kinds.

Unified integer can store its value in three c-types depending on value itself: `char`, `int` and special big integer handling integers of arbitrary values.

# Predicates

We use predicates as a main term reasoner in laurelang. Predicates have a header and unlimited number of bodies (cases). If one case is considered invalid for relation we go to the next case, and we won't stop searching for all of possible solutions until we won't be asked to.

Let's start with a simple zero-relation predicate.

*Cool number can be 0, 42, 110 and 2*

```laurelang
: ?cool_number(int).
?cool_number(0).
?cool_number(42).
```

In first line we declare the relation universum: universum for every argument of a predicate. If the universum wont be constrainted, all possible values of universum will be considered a solution.

We have a bunch of predicates, or statements that cool number is 0, 42, 110 or 2. That means that we have multiple solution for cool number. Now if we query:

```laurelang
| cool_number(x)
x = 0
```

repl will offer us to continue the search.

```laurelang
x = 0;
x = 42.
```

> **Search operators**. In repl common symbols for solutions management are `;` which means search for more and `.` which says that that's all. Notice that repl concept is to show more specific variant of query passed, so if we pass `;` when there is no solutions more, repl will show `false` (because there is no more solutions) and if we pass `.` when there are some solutions left, repl will show `false` too (because there are some more solutions possible).  
> Also there is a `...` search operator which means "I don't care" for more solutions, interpreter will respond `true` both if solutions exists and not

Predicates can have a response argument like this:

```laurelang
: ?pair(int) -> int.
?pair(1) -> 5.
?pair(x) -> x {
    x > 10;
}
?pair(x / 2) -> x + 1.
```

Second variation can be rewritten with `when` keyword:

```laurelang
?pair(x) -> x when x > 10.
```

# Auto predicate

Auto predicate header is declared with `ID` particle.

```
: ?autopred(string) -> ID.
```

After declaration predicate response is generated automatically for each predicate case. In standard implementation it has a form of UUID.

```
id ~ autopred;
```

instance of auto predicate copy can be asserted to exact UUID (in string representation):

```
id = "2b7b9cff-335d-4d1d-9073-7c99b6fe8923"
```

id cannot be asserted to uuid which is not bound to declared predicate, so uuid assertion operation can be also used as existance check.

UUID can be used as a pointer to specific predicate case:

```
id = autopred(x);
```

## Case break

Monotonicity of predicate case search can be broken with `%` break char.

It is placed right after predicate case declaration (usually after `}` or `.`).

It basically means: do not search other cases if this one succeeds.

## Predicate notation

> **Infix arity** - arity of predicate excluding response argument

If infix arity of predicate equals to 1 then we can write down predicate like this:
```laurelang
length(List) = L
-- can be written as
length of List = L
```

If infix arity of predicate equals to 2 then we can write predicate name between two infix args:
```laurelang
+(1, 2) = x
-- can be written as
1 + 2 = x
```

Bearing in mind such notation may help you choosing the appropriate name for predicate.

# Useful predicates


## Implication

You can use implify operation like this:

```laurelang
?absolute(x) -> y {
    x >= 0 -> y = x;
    x < 0 -> y = -x;
}
```

## Predicate `fail()`

Fail predicate always fails. You can use it with implication to negate statements:

```laurelang
2 ^ x > 1000 -> fail();
```

## Cut

```
!
```

Predicate which always succeeds and cannot be backtracked.  
Other predicate variations won't be runned.

```laurelang
?pred(x) -> 5 when x > 10, x never 18, !.
?pred(x) -> 42.

y = pred(15) -> y = 5;
```

# Constraints

Constraints are used to operate on domains, so arguments of constraints are usually not instantiated. There are some builtin constraint operations:

* `>`
* `<`

There is no overload for builtin constraint operations in laurelang, so thay have default meaning for each datatype. Lets get acquainted with meaning for gt constraint: for integer the meaning is obvious, for array: array in the left side is greater by length than array in the right side, for structure: all structure elements are greater than same elements in another structure, for others it is undefined.

## Predicate `never(a, b)`

Standard library predicate `never` is partial logic contraint predicate implemented through implication. This predicate says that `a` could never be equal to `b`. So domain `(-inf;inf)` to instantiated integer `4` will result in failure, because domain `a` can be instantiated to integer `4`.

```
<1..5> never 4 -> fail();
<1..5> never 5;
```
## Example

*A is always fewer than B and never equal to C*

We may declare constraint sets in our database, we use heading-symbol `#` for this:

```laurelang
# my_constraint(A, B, C) {
    A > B;
    A never C;
}
```

In constraint sets only constraints and other constraint sets may be called: no predicates, no new instances. Only constraints.

# CLE (C logic enviroment)

Environment is essential for logic programming because we live in world full of procedural technologies and we have to make bridges for procedure driven events to be reasoned by our logic machine.

Laurelang provides an API for these needs, API documentation can be found [click](/docs/cle/index.md).

# Choice point

Choice point is created each time predicate is called, but it also can be created via choice operation. Choice operation consists of different possible expression variations divided by `|` symbol, it also can be applied to other operations recursively.

```laurelang
{
-- a is 1, 2 or 1 + 2
a = 1|2|1 + 2;
a + b = 5;
}
```

Pairs for (a, b) will be found:
```
a = 1, b = 4;
a = 2, b = 3;
a = 3, b = 2.
```

> Operations which **do not** create a choice-point are imaging and calling a constraint

# Structures

Structure is a cartesian product of it's sets. It's declaration is simple: `$`, name of the struct, `{`, name declarations, `}`

```laurelang
$ my_struct {
    int x;
    int y;
}
```

Aside from name declarations, structure body may contain constraint calls.  
Such as `x > 0; y > 10`, try adding them to structure body.

```laurelang
-- in order to create a structure imaging may be used
a ~ my_struct;
-- a representation is {(0, inf), (10, inf)}

-- try to unify manually
a = {0, 0};
-- interpreter says `false`, 
-- value is not valid for structure sets

-- try to partially unify manually
a = {5, _};
-- a representation is {5, (10, inf)}

-- request a unification
a?
-- interpreter responds
a = {5, 11};
a = {5, 12};
a = {5, 13}...
```

## Structure referring to itself

Structure referring to itself may never get unified, so there is some kind of optimization of representing such structures (the recursive part is shown as ellipsis after the first iteration)

```laurelang
$ person {
    string name;
    person mother;
}

-- shown as
{(string of any length), {(string of any length), ...}}
```

# Package

## What package consists of

Package may be single-file or a folder with init file inside it.

Init file should be named same as package folder, extension is `.le`.

```
my_package
   - my_package.le
   - first_tool.le
   - second_tool.le
```

Usually header file just include other public package resources.

## How to include

Use command `use`:

```
!use "my_package", <http>.
```

There can be any number of packages to include declared.
The `@` symbol means that package is the global package (such as std).

## How to include CLE packages

Use command `useso`.