: ?fac(natural*) -> natural.

?fac(0) -> 1.
?fac(n) -> f {
    n?; n > 0;
    f = n * fac(n - 1);
}