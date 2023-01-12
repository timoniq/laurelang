: ?generic1('T', 'T'[]).
?generic1(a, b) {
    each of b = a;
}

-- 1
: ?test_generic(int).
?test_generic(x) {
    char[] arr = ['a', 'b', 'c'];
    generic1(x, arr);
}
?test_generic(x) {
    generic1(x, [1]);
}

-- empty constraint
: #generic2('T', 'T'[]).
#generic2(_, _).

: ##generic2_sc('T', 'T'[]).

-- 10
: ?test_generic_t_check(int).
?test_generic_t_check(x) {
    generic2(x, 1);
    generic2_sc(x, 1);
    x = 2;
}
?test_generic_t_check(x) {
    generic2(x, [1,2]);
    generic2_sc(x, [1,2,3]);
    x = 10;
}
?test_generic_t_check(x) {
    generic2(x, ['a', 'b']);
    generic2_sc(x, ['c', 'd', 'e']);
}

: ##deepresolve((?('C', (?('A', 'B')))), 'B') -> 'A'.
: ?resolutor(int, (?(int, string))).

-- "(string), <-inf..inf>"
: ?test_generic_deepresolve(string).
?test_generic_deepresolve(result) {
    deepresolve(resolutor, x) = y;
    !lock y;
    result = format("{x}, {y}");
}

: ?same('T') -> 'T'.
?same(x) -> x.

: ?test_clarification_1().
?test_clarification_1() {
    map{int}([-1,-2,-3], absolute) = [1,2,3];
    map{char}(['a', 'b', 'c'], absolute) = ['a', 'b', 'c'] -> fail();
    map([1,2,3], same{int}) = x;
    x = [1,2,3];
}
?test_clarification_1() {
    m ~ map{int};
    repr(m) = "(?m(int[], (?(int) -> int)) -> int[])";
}
?test_clarification_1() {
    m ~ map{int[]};
    repr(m) = "(?m(int[][], (?(int[]) -> int[])) -> int[][])";
}

: ?increpr(int) -> string.
?increpr(x) -> repr(x + 1).

: ?test_clarification_2().
?test_clarification_2() {
    -- should fail because absolute does not correspond (?(int) -> string) signature
    map{T=int, R=string}([1,2,3], absolute) = x -> fail();
    -- should succeed
    map{T=int, R=string}([1,2,3], increpr) = x;
    x = ["2", "3", "4"];
}

-- "#dr((?(string[], (?('A', int)))), int) -> 'A'"
: ?test_clarification_by_repr(string).
?test_clarification_by_repr(x) {
    dr ~ deepresolve{B=int, C=string[]};
    x = repr(dr);
}