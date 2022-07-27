# bagof predicate

known as

* `from --> to`
* `to <-- from`
* `to = bagof(from)`

bagof is needed to collect solutions for a `from` variable into `to` array.

bagof has a solution for array for each solution for `from` variable.

# sized_bagof predicate

needed to include size of bag into this relation.

written as:

```
sized_bagof(size, from) = to
// example:
3 sized_bagof x = arr
```

# examples

string `lower` predicate from string reasoning example:

```
: ?lower(string) -> string.
?lower(s) -> l {
    c = lower_char(s[_]);
    l = (length of s) sized_bagof c;
}
```

explanation:

1) c is lowered char of s with anonymous index (see original example for `lower_char` implementation)
2) l is x sized bag of `c` where x is length of `s` string (not lowered string)