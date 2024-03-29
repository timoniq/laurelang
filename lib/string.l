-- concatenates two strings
: ?concat(string, string) -> string.
?concat(a, b) -> append(a, b).

-- joins two strings with a joiner
-- try:
-- `join_two(", ", "apples", "tomatoes") = x`
: ?join_two(string, string, string) -> string.
?join_two(_, a, "") -> a.
?join_two(_, "", b) -> b.
?join_two(joiner, a, b) -> flatten([a, joiner, b]).

-- join strings by joiner
-- try:
-- `join(" or ", ["happy", "sad", "puzzled", "carefree"]) = x`
: ?join(string, string[]) -> string.

-- after some optimizations in auto query ordering
-- and `append` predicate (for `join_two`) impovement 
-- (iteration with only result known) - this `join` one will be complete.
?join(_, []) -> "".
?join([], s) -> flatten{string}(s).
?join(_, list) -> list[0] when (length of list) = 1.
?join(joiner, s) -> joined {
    (length of s) > 1;
    linked(head, tail) = s;
    tail_s = join(joiner, tail);
    joined = join_two(joiner, head, tail_s);
}
