# Constraints

Constraints are used to operate on domains, so arguments of constraints are usually not instantiated. There are some builtin constraint operations:

* `>`
* `<`

There is no overload for builtin constraint operations in laurelang, so thay have default meaning for each datatype. Lets get acquainted with meaning for gt constraint: for integer the meaning is obvious, for array: array in the left side is greater by length than array in the right side, for structure: all structure elements are greater than same elements in another structure, for others it is undefined.

## Predicate `cons(a, b)`

Standard library predicate cons is partial logic contraint predicate implemented through implication. This predicate says that `a` could never be equal to `b`. So domain `(-inf;inf)` to instantiated integer `4` will result in failure, because domain `a` can be instantiated to integer `4`.

```
<1..5> cons 4 -> fail();
<1..5> cons 5;
```
## Example

*A is always fewer than B and never equal to C*

We may declare constraint sets in our database, we use heading-symbol `#` for this:

```laurelang
# my_constraint(A, B, C) {
    A > B;
    A cons C;
}
```

In constraint sets only constraints and other constraint sets may be called: no predicates, no new instances. Only constraints.