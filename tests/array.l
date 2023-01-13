!use std:arrays.

-- [1, 2, 3]
: ?test_array_basic(int[]).

-- [5, 4, -5]
-- [5, 5, -5]
: ?test_array_with_domain(int[]).

?test_array_basic(arr) {
    int[][] _a = [100, 200, 300] -> fail();
    int[][] _a = [[1, 2], [3]];
    arr = [1, 2, 3];
}

?test_array_with_domain(arr) {
    int c = <3..5;
    arr = [5, c, -5];
    arr?;
}

-- 1
-- 2
-- 3
: ?test_array_each(int).

?test_array_each(x) {
    int[] arr = [1, 2, 3];
    x = each of arr;
}

-- [1, 2]
-- [3, 4]
-- [5]
: ?test_array_each_2(int[]).

?test_array_each_2(element) {
    int[][] arr = [[1, 2], [3, 4], [5]];
    int[] x = [1, 2];
    each of arr = x -> fail();
    int[][] arr2 = [[1, 2], [1, 2], [1, 2]];
    each of arr2 = x;
    int one = 1;
    each of arr2 = one -> fail();
    int[] arr3 = [1, 1, 1];
    each(arr3) = one;
    element = each(arr);
}

-- []
-- [1]
-- [1, 1]
-- [1, 1, 1]
-- ...
: ?test_array_each_gen_array(int[]).

?test_array_each_gen_array(arr) {
    int element = 1;
    element = each(arr);
    arr?;
}

-- 5
-- -1
-- 3
-- 5
: ?test_array_by_idx(int).

?test_array_by_idx(x) {
    int[] arr = [1, 2, 3, 5, 8, 13, 21];
    arr by_idx 3 = x;
    y ~ int;
    arr by_idx x = y;
    &all y {
        y = 13;
    }
}

?test_array_by_idx(-1).

?test_array_by_idx(x) {
    int[] arr = [1, 2, 3, 5, 8, 5, 3, 2, 1];
    int el = 5;
    arr by_idx x = el;
}

: ?test_array_length().
?test_array_length() {
    int[] arr = [100, 200, 300];
    length of arr = x;
    &all x {x = 3};
}

-- [2, 2, 2]
-- [2, 2, 3]
-- [2, 3, 2]
-- [2, 3, 3]
-- [3, 2, 2]
-- [3, 2, 3]
-- [3, 3, 2]
-- [3, 3, 3]
: ?test_array_length_gen_array(int[]).

?test_array_length_gen_array(x) {
    int el = <1..3;
    arr ~ el[];
    length of arr = 3;
    x = arr;
}


-- [];0
-- [3];1
-- [4];1
-- [5];1
-- [3, 3];2
-- [3, 4];2
-- [3, 5];2
-- [4, 3];2
-- ...
: ?test_array_length_genall(int[], int).

?test_array_length_genall(final, len) {
    int el = 3..5;
    arr ~ el[];
    length of arr = len;
    final = arr;
}

-- 2
-- 1
-- 5
: ?test_array_by_idx_syntax(int).
?test_array_by_idx_syntax(x) {
    int[] arr = [1, 2, 3];
    arr[1] = x;
}
?test_array_by_idx_syntax(idx) {
    int[] arr = [1, 4, 3, 8, -4, 4];
    int seek = 4;
    arr[idx] = seek;
}

: ?test_array_no_repeat().
?test_array_no_repeat() {
    no_repeat{int}([1, 2, 3]);
    no_repeat{int}([1, 2, 2]) -> fail();
    no_repeat{int}([1, 1, 2]) -> fail();
    no_repeat{int} of [];
    no_repeat{int} of [1];
    no_repeat{int} of [1, 2];
    no_repeat{int} of [1, 1] -> fail();
}


-- [1, 2, 3, 4, 5]
-- []
-- [1]
-- [3]

: ?test_array_append(int[]).

?test_array_append(x) { append{int}([1,2], [3,4,5]) = x }
?test_array_append(x) { append{int}([], []) = x }
?test_array_append(x) {
    append{int}([1,2], [3]) = [1] -> fail();
    append{int}(x, [2,3]) = [1,2,3];
}
?test_array_append(x) {
    append{int}([1, 2], x) = [1, 2, 3];
}

-- [1, 2, 3, 4, 5]
: ?test_array_flatten(int[]).
?test_array_flatten(arr2) {
    int[][] arr = [[1,2], [], [3,4,5]];
    arr2 = flatten(arr);
}

-- []
-- [1, 2]
-- [1, 2, 3]
-- [1, 1, 2, 3]
-- [1, 2, 1, 2, 3]
-- [3, 4]
: ?test_array_head_tail(int[]).
?test_array_head_tail([|]).
?test_array_head_tail([1,2|]).
?test_array_head_tail([|[1,2,3]]).
?test_array_head_tail([1|[1,2,3]]).
?test_array_head_tail([1,2|[1,2,3]]).
?test_array_head_tail(x) {
    [1, 2, 3, 4] = [1, 2|x];
}


-- 1;[2, 3]
-- 2;[3]
: ?test_array_head_tail_resolve(int, int[]).
?test_array_head_tail_resolve(x, y) {
    int[] arr = [1, 2, 3];
    arr = [h|t];
    x = h;
    y = t;
}
?test_array_head_tail_resolve(x, y) {
    int[] arr = [1, 2, 3];
    arr = [a, b|t];
    x = b;
    y = t;
}

-- [2, 2, 2]
-- [5, -10, 3]
-- [5, -10, 4]
: ?test_array_sized(int[]).
?test_array_sized(arr) {
    x = between(1, 3);
    a ~ x[3];
    a?;
    arr = a;
}

?test_array_sized(arr) {
    a ~ int[3];
    a[0] = 5;
    a[1] = -10;
    x = between(2, 5);
    a[2] = x;
    a?;
    arr = a;
}

-- 4
: ?test_array_nested_by_idx(int).
?test_array_nested_by_idx(x) {
    int[][] arr = [[1,2,3],[4,5,6]];
    x = arr[1][0];
}

-- [1, 2, 3]
: ?test_array_to(int[]).
?test_array_to(x) {
    int[] arr = [1,2,3,4,5];
    arr to 3 = x;
}

-- [4, 5]
: ?test_array_from(int[]).
?test_array_from(x) {
    int[] arr = [1,2,3,4,5];
    arr from 3 = x;
}

-- ["def", "foo"]
: ?test_array_slice(string[]).
?test_array_slice(x) {
    string[] arr = ["abc", "def", "foo", "bar"];
    slice(arr, 1, 3) = x;
}

-- [];[1, 2, 3]
-- [1];[2, 3]
-- [1, 2];[3]
-- [1, 2, 3];[]
: ?test_array_append_combinations(int[], int[]).
?test_array_append_combinations(x, y) {
    int[] arr = [1,2,3];
    arr = append(x, y);
}

-- [4, 5, 6]
-- [4, 3, 5]
: ?test_array_pkg_longest(int[]).
?test_array_pkg_longest(x) {
    longest{int}([[1,2],[],[3],[4,5,6],[3, 2],[4,3,5],[]]) = x;
}

-- [3, 2, 1]
-- [1, 2, 3]
: ?test_array_pkg_reverse(int[]).
?test_array_pkg_reverse(
    reverse{int}([1,2,3])
).
?test_array_pkg_reverse(x) {
    [3, 2, 1] = reverse{int}(x);
}

: ?test_array_pkg_not_empty().
?test_array_pkg_not_empty() {
    not_empty{int}([1,2,3]);
    not_empty{int}([]) -> fail();
}

-- [1]
-- [1, 2]
-- [1, 2, 3]
-- [2]
-- [2, 3]
-- [3]
: ?test_array_pkg_subarray(int[]).
?test_array_pkg_subarray(x) {
    subarray{int}([1,2,3]) = [2,3];
    subarray{int}([1,2,3]) = [] -> fail();
    subarray{int}([1,2,3]) = x;
}

-- [[1, 2]]
-- [[3, 4, 5], [6]]
: ?test_array_pkg_tail_or_all(int[][]).
?test_array_pkg_tail_or_all(x) {
    x = tail_or_all{int}([[1,2]]);
}
?test_array_pkg_tail_or_all(x) {
    x = tail_or_all{int}([[1,2], [3,4,5], [6]]);
}