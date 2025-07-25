: ?test_string().

?test_string() {
    string s = "abc def";
    s = "abc fed" -> fail();
    s = "abc def";
    string e = "abc def";
    s = e;
    string f = "abc fed";
    s = f -> fail();
}

-- 'т'
-- 'δ'
: ?test_string_unicode(char).
?test_string_unicode(ch) {
    string s = "Жертвоприношение";
    c ~ char;
    s by_idx 3 = c;
    ch = c;
    length of s = 16;
}
?test_string_unicode(ch) {
    ch = 'δ';
}

: ?test_string_array().
?test_string_array() {
    string str = "aaaaaaa";
    char ch = 'a';
    each of str = ch;
}

-- 0
-- 2
-- 3
: ?test_string_array2(int).
?test_string_array2(idx) {
    string str = "lollipops";
    char ch = 'l';
    str[idx] = ch;
}

-- 0;'l'
-- 1;'ø'
-- 2;'v'
-- 3;'e'
-- 0;'?'
: ?test_string_chars(int, char).
?test_string_chars(i, x) {
    string s = "løve";
    s[i] = x;
}
?test_string_chars(0, x) when x?.

: ?concat(string, string) -> string[].
?concat(a, "") -> [a].
?concat(a, b) -> [a, " ", b] {
    (length of b) > 0;
}

: ?join(string[]) -> string.
?join([]) -> "".
?join(x) -> y {
    linked(head, tail) = x;
    other = join(tail);
    string[] arr = concat(head, other);
    flatten{char}(arr) = y;
}

-- "hello world of frustrations"
-- "изысканный"
: ?test_string_join(string).
?test_string_join(s) {
    s = join(["hello", "world", "of", "frustrations"]);
}
?test_string_join(join(["изысканный"])).