!use std:abstract.

-- <-inf..inf>
: #test_abstract_primitive(int).
#test_abstract_primitive(x) {
    int p = <0..3;
    arr ~ p[];
    abstract_primitive(arr) = x;
}

-- [<-inf..inf> x 0..inf>]
: #test_abstract_nesting(int[]).
#test_abstract_nesting(x) {
    p > 0;
    arr ~ p[][];
    abstract_nesting(x) = arr;
}

: ##translator_test('T', 'R', (?('T', (?('T'[]) -> 'R'[]))  -> 'R')).

-- "?(int, (?(int[]) -> char[])) -> char"
-- "#translator_test('T', 'R', (?('T', (?('T'[]) -> 'R'[]))  -> 'R'))"
: #test_abstract_type_resolver(string).
#test_abstract_type_resolver(r) {
    int_ ~ int;
    char_ ~ char;
    translator_test(int_, char_, x);
    r = repr(x);
}
#test_abstract_type_resolver(repr(translator_test)).