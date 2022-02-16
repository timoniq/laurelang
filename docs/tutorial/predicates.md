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

## Predicate notation

> **Infix arity** - arity of predicate excluding response argument

If infix arity of predicate equals to 1 then we can write down predicate like this:
```laurelang
length(List) = L
// can be written as
length of List = L
```

If infix arity of predicate equals to 2 then we can write predicate name between two infix args:
```laurelang
+(1, 2) = x
// can be written as
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

## fail()

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
?pred(x) -> 5 when x > 10, x cons 18, !.
?pred(x) -> 42.

y = pred(15) -> y = 5;
```