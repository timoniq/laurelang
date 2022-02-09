# Multiplicity Image

Multiplicity is a wrapper over images of instantiated datatype.

Int multiplicity may be declared as:

```
int a = {1, 2, 3}
```

Multiplicity may not store any ambiguative images. Multiplicities may be declared in predicate call:

```
a = {1, 8} + 3
```

Here choice point will be generated.

To manually generate a choicepoint from multiplicity unify-operator may be used:

```
int a = {1, 13, 54};
a?;
message(repr(a));
```