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