-- [1]
-- [1, 2]
-- [1, 2, 3]
-- []
-- [2]
: ?test_bagof_1(int[]).

?test_bagof_1(bag) {
    x = 1 .. 3;
    x?;
    x --> bag;
}

?test_bagof_1([]).
?test_bagof_1(bag) {
    bag <-- ( between(1, 3)? );
}

-- 1
-- 2
-- 3
: ?test_bagof_2(int).
?test_bagof_2(x) {
    bagof(x) = [1, 2, 3];
}
?test_bagof_2(x) {
    bagof(x) = [];
}

: ?test_bagof_3(int[]).
?test_bagof_3(x) {
    bagof{int}(1) = x;
    bagof{int}(x[0]) = x;
}

-- [1, 2, 3]
: ?test_sized_bagof(int[]).
?test_sized_bagof(bag) {
    sized_bagof(3, (1 .. 5)?) = bag;
}

-- [0, 1, -1]
-- [1, 1, 2, 6, 24, 120, 720, 5040, 40320, 362880]
: ?test_sized_bagof_stop(int[]).
?test_sized_bagof_stop(bag) {
    a ~ int;
    a?;
    3 sized_bagof a = bag;
}
?test_sized_bagof_stop(bag) when 10 sized_bagof fac(_) = bag.

-- 1;[2]
-- 2;[2, 3]
-- 3;[2, 3, 4]
: ?test_sized_bagof_2(int, int[]).
?test_sized_bagof_2(n, bag) {
    x = between(1, 5)?;
    sized_bagof(n, x) = bag;
}