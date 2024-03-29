: ?test_math_basic().
: ?test_math_multiply().
: ?test_math_pow().
: ?test_math_divide().

-- 1
-- 5
: ?test_math_plus_minus(int).

-- 11
-- -11
: ?test_math_absolute(int).

-- 9
-- 4
-- 0
-- 4
-- 3
: ?test_math_sqrt(int).

: ?test_gt().
?test_gt() {
    1 > 0;
    11 > 4;
    11 > 12 -> fail();
    11 < 14;
    1..2 > 0;
    1..2 >= 0;
    1..2 >= 1;
    1..2 >= 2;
    1..2 >= 3 -> fail();
}

: ?test_bigint().

?test_math_basic() {
    2 + 2 = 4;
    a + 1 = 2;
    2 - 2 = 0;
    a = 1;
    2 - a = a;
    b ~ int;
    b > 0;
    a __gte b;
    &all b {
        b = 1;
    };
}

?test_math_multiply() {
    2 * 3 = 6;
    4 * x = 32;
    &all x {
        x = 8
    };
    x * x = 64;
}

?test_math_divide() {
    5 / 1 = 5;
    5 / 5 = 1;
    200 / 4 = x;
    &all x {x = 50};
}

?test_math_pow() {
    2 pow 10 = 1024;
    0 pow 0 = 1;
    7 pow 1 = 7;
}

?test_math_plus_minus(x) {
    1 +- 1 = 0;
    1 +- 1 = 2;
    x +- 2 = 3;
}

?test_math_absolute(x) {
    absolute(0) = 0;
    absolute(-5) = 5;
    absolute(x) = 11;
}

?test_math_sqrt(x) when
    x = sqrt of 81.
?test_math_sqrt(x) when
    2 = sqrt of x.
?test_math_sqrt(0) when
    10 = sqrt of 10 pow 2.

?test_math_sqrt(sqrtu(10)).
?test_math_sqrt(sqrtd(10)).

?test_bigint() {
    2 pow 50 = 1125899906842624;
    int x = 3041409320171337804361260;
    x * 16 = 48662549122741404869780160;
}

: ?test_natural().

?test_natural() {
    n ~ natural;
    n < 10;
    &all n { n > 0; };
}

: ?test_negate().

?test_negate() {
    int x = 10;
    y = -x;
    &all y { y = -10 };
}

-- 42
-- 87
: ?test_chain(int).

?test_chain(x) when
    1 + 4 + 45 - 4 * 2 = x.
?test_chain(x) when 
    1 + 4 + (45 - 4) * 2 = x.