!use string_reasoning.

-- example of map usage

-- single choice point
: ?inc(int) -> int.
?inc(a) -> a + 1.

-- double choice point
: ?mapping(int) -> int.
?mapping(a + 1) -> a.
?mapping(a + 100) -> a.

: ?same('T') -> 'T'.
?same(x) -> x.

-- try these simple queries:
-- map{int}([1,2,3], inc) = [2,3,4]
-- map{int}(x, mapping) = [1,2,3]

string[] listing = [
    "strives", "efficient", "to", 
    "laurelang", "as", "be", 
    "possible", "as"
].

: ?crypt(natural*) -> string.
?crypt(i) -> listing[i].

*--
try such mapping as an example:
map{int, string}([3, 0, 2, 5, 4, 1, 4, 6], crypt) = x

try constrainting your call with map type
 int ltz = ..0, 
 map{T=ltz}([-1, 2, -5], absolute) = x
should fail
 int ltz = ..0,
 map{T=ltz}([-1, -2, -5], absolute) = x
should succeed (x = [1, 2, 5])
--*

mapints ~ map{int}.

-- try mapping with same
-- mapints([1,2,3], same) = x

$ kv {
    string k;
    int v;
}

dict ~ kv[].

dict dictionary = [("a", 3), ("foo", 100), ("bar", 10), ("test", 10)].

: ?by_key(dict, string) -> int.
?by_key(d, k) -> v {
    d[i] = x;
    x = (k, v);
}

$ kvr {
    int k;
    string v;
}

: ?kv2kvr(kv) -> kvr.
?kv2kvr((k, v)) -> (v, k).

: ?dict_swap(dict) -> kvr[].
?dict_swap(d) -> dr {
    map(d, kv2kvr) = dr;
}