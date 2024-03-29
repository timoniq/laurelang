: ?test_NtoZ().
: ?test_fibonacci().
: ?test_factorial().
: ?test_nope().
: ?test_factorial_row().

-- 10
-- 2
: ?test_with_when(int).

?test_NtoZ() {
    nz(0) = 0;
    nz(1) = 1;
    nz(9) = 5;
    nz(10) = x;
    x = -5;
}

?test_fibonacci() {
    5 = fib(5);
    x = fib(7);
    &all x {x = 13};
}

?test_factorial() {
    fac(0) = _a;
    &all _a {_a = 1};
    fac(60) = 8320987112741390144276341183223364380754172606361245952449277696409600000000000000;
}

?test_factorial_row() {
    i > 0;
    &exists i {
        fac(i) = 120;
        &all i {
            i = 5;
        };
    };
}

?test_nope().

?test_with_when(x) when x = 10.
?test_with_when(x) when x = 2.
?test_with_when(x) when x = 3, fail().

: ?test_predcall_eq_predcall().

?test_predcall_eq_predcall() {
    1 + 3 = a + 1;
    &all a {a = 3};
}

?test_predcall_eq_predcall() {
    n < 20;
    fac(10) = fac(n);
    &all n {n = 10};
}

-- 3;10
-- 6;7
: ?test_choice_sets(int, int).
?test_choice_sets(x, y) {
    {
        x = 3;
        y = 10;
    } | {
        x = 6;
        y = 7;
    };
}

: ?test_prime_check().
?test_prime_check() {
    prime(2);
    prime(5);
    prime(7);
    prime(4) -> fail();
}

-- 5
-- 7
-- 11
-- 13
-- 17
-- 19
: ?test_prime_gen(int).
?test_prime_gen(n) {
    n = between(3, 20);
    prime(n);
}

-- "<-inf..inf>"
-- "?test_repr(string)"
-- "101"
: ?test_repr(string).
?test_repr(x) {
    i ~ int;
    repr of i = x;
}
?test_repr(repr of test_repr).
?test_repr(x) {
    int a = 101;
    repr of a = x;
}

-- 125
-- 42
: ?test_repr_inverted(int).
?test_repr_inverted(x) {
    repr of x = "125";
}
?test_repr_inverted(x) {
    repr of x = "42";
}