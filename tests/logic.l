: ?test_imply().

-- -3
: ?test_imply_nested(int).
: ?test_choicepoint().

-- 1
-- 2
-- 3
-- 4
: ?test_unify(int).

-- 1;-3
-- 1;10
-- 5;-3
-- 5;10
: ?test_combine(int, int).

: ?test_fail().

-- 10
-- -3
: ?test_nested_1(int).

-- 5
-- 4
-- 8
-- 12
-- 16
-- ...
: ?test_nested_2(int).

-- 5
-- -100
-- 5
-- 42
-- -100
-- 42
: ?test_cut(int).

: ?test_infer().
?test_infer() {
    int ~ int;
    int ~ char -> fail();
    x ~ int;
    x ~ int[] -> fail();
    arr ~ char[];
    arr[] ~ char[][];
    arr2 ~ arr[];
    arr2 ~ char[][];
}

?test_imply() {
    int x = 10;
    y ~ int;
    x > 20 -> y = 2;
    x > 10 -> y = 3;
    x = 10 -> y = 4;
    &all y {y = 4};
}

?test_imply_nested(x) {
    int y = 11;
    y > 5 -> {
        y < 11 -> x = 1;
        y > 7 -> y > 10 -> x = -3;
    };
    1 + 1 = 3 -> 1 + 1 = 4;
    1 + 1 = 3 -> 2 + 2 = 6 -> fail();
}

?test_choicepoint() {
    x ~ int;
    x = 11|22;
    x = 11 * n;
    x = 11 -> n = 1;
    x = 22 -> n = 2;
}

?test_unify(x) {
    x > 0;
    x < 5;
    x?;
}

?test_combine(x, y) {
    x = 1|5;
    y = -3|10;
}

?test_fail() {
    fail() -> fail();
}

-- 6
-- 7
-- 8
-- 9
: ?test_intersection(int).
?test_intersection(x) {
    x > 5;
    y < 10;
    x = y;
}

: ?nested_help(int, int).
?nested_help(1, 4).
?nested_help(x, x) when x > 0.
?nested_help(10, -3).
?nested_help(x, x - 8) when x < 0.

?test_nested_1(x) {
    nested_help(20, -3) -> fail();
    nested_help(4, 1) -> fail();
    nested_help(1, 4);
    nested_help(-1, -9);
    nested_help(100, 100);
    nested_help(10, x);
    x?;
}

: ?nested_help2(int) -> int.
?nested_help2(x) -> 5 when nested_help(x, 4).
?nested_help2(7) -> 7.
?nested_help2(x) -> x when x +- 4 = 10.
?nested_help2(4) -> x when n > 0, x = 4 * n.

?test_nested_2(x) {
    nested_help(a, 4);
    x = nested_help2(nested_help2(a));
}

: ?cut_help(int) -> int.
?cut_help(x) -> 5 when x > 10, x never 18, x > 12 -> !.
?cut_help(x) -> 42.

?test_cut(x) {
    cut_help(15) = x;
}
?test_cut(-100).
?test_cut(x) {
    cut_help(12) = x;
}
?test_cut(-100).
?test_cut(x) {
    cut_help(3) = x;
    cut_help(-10) = x;
    cut_help(18) = x;
}

: ?test_empty_set().
?test_empty_set() when {{{{{{{{}}}}}}}}.
?test_empty_set() {
    {{{{{{}}}}}};
}

-- 14
-- 40
: ?test_pred_prim(natural).
?test_pred_prim(x) {
    prim ~ (?(int, int) -> int);
    prim = +|-|*;
    x = 4 prim 10;
}

: ?test_pred_prim_solve().
?test_pred_prim_solve() {
    primitive ~ (?(int) -> int);
    primitive?;
    primitive(-100) = 100;
    primitive(8 - 10) = x;
    &all x { x = 2 };
}

: ?case_of_cut(int) -> int.
?case_of_cut(42) -> 1%
?case_of_cut(x) -> 2 when x / 2.

-- 1
-- 123
-- 2
: ?test_case_of_cut(int).
?test_case_of_cut(x) {
    case_of_cut(42) = x;
}
?test_case_of_cut(123).
?test_case_of_cut(x) {
    case_of_cut(36) = x;
}%
?test_case_of_cut(100).
