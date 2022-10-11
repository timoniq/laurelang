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

# predicate call grounding

Predicate order is important to build an effective query.

`marriage` is a predicate containing fixed relations of person to person.

```
marriage(person(name, _), person(_, age))
```

in the example above query is quite inefficient if name and age are newly declared variables and are abstract.

person predicate calls are placed before the marriage predicate call by default, this is important to remember in order to build efficient queries.

to change the default unpacking of predicate calls, person predicate calls can be grounded like this:

```
marriage('person(name, _), 'person(_, age))
```

this will place them after the marriage predicate call, so all the people won't be iterated in order to find pair of ones who belong to marriage relation.