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
// in order to create a structure imaging may be used
a ~ my_struct;
// a representation is {(0, inf), (10, inf)}

// try to unify manually
a = {0, 0};
// interpreter says `false`, 
// value is not valid for structure sets

// try to partially unify manually
a = {5, _};
// a representation is {5, (10, inf)}

// request a unification
a?
// interpreter responds
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

// shown as
{(string of any length), {(string of any length), ...}}
```