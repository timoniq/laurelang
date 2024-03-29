: ?fib(int) -> int.

?fib(0) -> 0.
?fib(1) -> 1.
?fib(n) -> f {
    n > 1; n?;
    f = fib(n - 1) + fib(n - 2);
}

*--
Creates an infinite row of fibonacci numbers
each solution for response argument is a fibonacci
number.
`fibr(0, 1) = f`
--*
: ?fibr(int, int) -> int.

?fibr(x, y) -> x + y.
?fibr(x, y) -> fibr(y, x + y).