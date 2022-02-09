# Constraints

Constraints are used to operate on domains, so arguments of constraints are usually not instantiated. There are some builtin constraint operations:

* `>` - left side is greater than right side
* `<` - left side is less than right side
* `#` - left side is never equal to right side, if smt is instantiated, it has priority of being a constraint

There is no overload for builtin constraint operations in laurelang, so thay have default meaning for each datatype. Lets get acquainted with meaning for gt constraint: for integer the meaning is obvious, for array: array in the left side is greater by length than array in the right side, for structure: all structure elements are greater than same elements in another structure, for others it is undefined.

Task: *A is always fewer than B and never equal to C*

We may declare constraint sets in our database, we use heading-symbol `#` for this:

```laurelang
# my_constraint(A, B, C) {
    A > B;
    B # C;
}
```

In constraint sets only constraints and other constraint sets may be called: no predicates, no new instances. Only constraints.