-- old syntax --

: ?sum(int[]) -> int.

?sum([h]) -> h.
?sum([h|tail]) -> h + sum(tail).

-- new syntax --

?sum(int[]) = int {
    [h] = h;  -- if predicate has single argument it can be written without (). this can be replaced with ([h]) = h
    ([h|tail]) = x {
        x = h + sum(tail);
    }
}