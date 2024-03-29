: ?inc(int) -> int.

!ordering.
: ?flatten_('T'[][]) -> 'T'[].

?inc(x) -> x + 1.

?flatten_([]) -> [].

?flatten_(array) -> flattened {
    linked(head, tail) = array;
    flattened = head:append(tail:flatten_());
}