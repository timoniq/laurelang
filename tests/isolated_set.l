-- 12
-- 13
-- 14
-- 13
-- 1
: ?test_isolated_set(int).

?test_isolated_set(x) {
    x > 11;
    #{x > 13};
    x < 15;
    x?;
}

?test_isolated_set(x) {
    #{x = 11};
    x = 13;
}

?test_isolated_set(x) {
    char c = 'l';
    #{c = 'p'} -> fail();
    #{x = 0};
    x = 1;
}