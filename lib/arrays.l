!ordering.
-- Determines array to its reverse
: ?reverse('T'[]) -> 'T'[].
?reverse([]) -> [].
?reverse(lst) -> reversed {
    linked(c, tail, lst);
    linkedr(reverse(tail), c) = reversed;
}

-- States that array is not empty
: ?not_empty('T'[]).
?not_empty([_|_]).

-- States that array has a subarray
-- ```
-- subarray([1,2,3,4,5]) = [2, 3]
-- subarray([1,2,3]) = x
-- ```
: ?subarray('T'[]) -> 'T'[].
?subarray(xs) -> sa {
    append(_, 'append(sa, _)) = xs;
    not_empty(sa);
}

-- Determines two arrays to longest one
: ?longest_of_two('T'[], 'T'[]) -> 'T'[].
?longest_of_two(x, y) -> x when length(x) >= length(y).
?longest_of_two(x, y) -> y when length(x) <= length(y).

-- Determines array of arrays to longest one
: ?longest('T'[][]) -> 'T'[].
?longest([]) -> []%
?longest(arr) -> l {
    linked(x, tail) = arr;
    m = longest(tail);
    l = longest_of_two(x, m);
}

-- Determines array w/o head or whole array 
-- if it doesn't obtain a tail.
: ?tail_or_all('T'[][]) -> 'T'[][].
?tail_or_all(x) -> y {
    linked(_, y, x);
    not_empty(y);
}%
?tail_or_all(x) -> x.

-- Relation between instance and nesting of its atom
-- ```
-- int[][] arr = [];
-- nesting_count(arr) = x
--     -> x = 2;
-- ```
: ?nesting_count('T') -> natural*.
?nesting_count(x) -> 0 {
    x ~ _[]
        -> fail();
}%
?nesting_count(xn) -> count {
    xn ~ x[];
    count = incr(nesting_count(x));
}

: ?without('T'[], 'T') -> 'T'[].
?without([], _) -> [].
?without([x|tail], wx) -> w {
    x = wx -> fail();
    append([x], without(tail, wx)) = w;
}%
?without([_|tail], x) -> without(tail, x).

: ?max_w_tail('T'[], 'T'[]) -> 'T'.
?max_w_tail(arr, tail) -> m {
    max(arr) = m;
    tail = without(arr, m);
}

-- Resolves instance to its atomic image to its dimension
: ?dim_image('T', Image, natural*).
?dim_image(ins, im, 0) {
    image(ins) = im;
    im = @array -> fail();
}%
?dim_image(ins, @array, n) {
    n = nesting_count(ins);
}