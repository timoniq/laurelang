: ?test_quantor_all().
: ?test_quantor_exists().

?test_quantor_all() {
    i ~ int;
    #{
        i = 2..4;
        &all i {
            prime(i);
        };
    } -> fail();
    #{
        i = 1..3;
        &all i {
            prime(i);
        };
    };
}

?test_quantor_exists() {
    int i = 0..5;
    &e i {
        prime(i);
        fib(i) = i;
    };
}