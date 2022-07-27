# syntax

# predicate, constraint

`?` leading sign is used to declare a predicate

```
: ?predicate(int).
?predicate(a) when a / 2.
```

`:` before predicate declaration is used to mark declaration as header.

header is needed to declare argument "types"

constraints use same syntax as predicates but with `#` sign instead of `?`.

# quantor

quantors use `&` leading sign.

quantors are:
* exists (E)
* All (A)

```
&quantor name {
    ...
}
```