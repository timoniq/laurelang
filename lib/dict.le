$Pair {
    string a;
    string b;
}

Dictionary ~ Pair[].

*--
Determines dictionary, key to its value.
```
Dictionary dict = [("foo", "bar"), ("abc", "def")];
dict_kv(dict, "abc") = x;
dict_kv(dict, y) = "bar";
```
--*
: ?dict_kv(Dictionary, string) -> string.
?dict_kv([pair|_], a) -> b {
    pair = (a, b);
}
?dict_kv([_|tail], a) -> dict_kv(tail, a).