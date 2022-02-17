# Builtin instances

## Addition predicate
### integer-add

`__+`, `+` in library.

```laurelang
10 + x = 15 -> x = 5;
```

## Subtraction predicate
### integer-subtract

`__-`, `-` in library.

```laurelang
10 - 5 = x -> x = 5;
```

## Integer constraints
### constraints

`__>` and `__gte`.  
`>` in library.

```laurelang
x > 0; x < 5; x?;
// 1, 2, 3, 4
```

# Test suite

Create predicates with name starting with `test_`. Add queries to test your code, useful constructions to test properly:

You may need to check that query doesn't create any ambiguation, so `&all` quantor may be used

```laurelang
x + 5 = 8;
&all x {x = 3};
```

You may need to require query to fail, so implication with `fail` may be used:

```laurelang
3 + 5 = 7 -> fail();
```

You may need to test query for multiple solutions, to do that, declare these solutions in the docstrings above the test predicate header, like this:

```laurelang
// 1
// 8
: ?test_my_predicate(int).
```

If predicate has multiple arguments, split them by `;`, like this: `10;20;30`. Do not use response arguments in test predicates. It is not important how you name arguments in test predicate variations.

## Error codes

Complicated error codes are: `GF1` (`GENERATOR_FAULT_VALUE`, which means that invalid solution was generated) and `GF2` (`GENERATOR_FAULT_COUNT`, which means that invalid count of solutions were generated).

Commentaries will be shown after all tests were ran.