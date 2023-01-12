-- "hello world"
-- "q100 abc"
-- "the (?+(int, int) -> int) predicate and a (string)"
-- ""
: ?test_formatting_1(string).
?test_formatting_1(y) {
    string x = "world";
    format of "hello {x}" = y;
}
?test_formatting_1(y) {
    int l = 100;
    format of "q{l} abc" = y;
}
?test_formatting_1(y) {
    format of "the {+} predicate and a {y}" = y;
}
?test_formatting_1(y) {
    format of "" = y;
}

-- 123
-- 42
: ?test_formatting_2(int).
?test_formatting_2(x) {
    format("abc--{x}hohoho") = "abc--" -> fail();
    format(";;{x}") = ";;123"
}
?test_formatting_2(x) {
    format("{x}") = "s" -> fail();
    format("{x}") = "42";
}