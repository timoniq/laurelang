$Pair {
    string a;
    string b;
}

*--
With abstract k/v types:
```
$Pair {
    'K' k;
    'V' v;
}
```
--*

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

: ?pair_value(Pair) -> string.
?pair_value((_, v)) -> v.

: ?pair_key(Pair) -> string.
?pair_key((k, _)) -> k.

: ?dict_values(Dictionary) -> string[].
?dict_values(dct) -> map(dct, pair_value).

: ?dict_keys(Dictionary) -> string[].
?dict_keys(dct) -> map(dct, pair_key).

-- Pairs dict values and keys
: ?dict_zip(string[], string[]) -> Dictionary.
?dict_zip(dict_keys(dct), dict_values(dct)) -> dct.
