!use std:arrays.

*--
relation between whole array,
excluded index and array with excluded index
`excluded_idx{int}([1,2,3], 2) = x`
--*
: ?excluded_idx('T'[], index_t) -> 'T'[].
?excluded_idx(arr, 0) -> narr {
    linked(_, narr) = arr;
}
?excluded_idx(arr, idx) -> narr {
    idx > 0;
    linked(el, tmp_arr) = arr;
    excluded_idx(tmp_arr, idx - 1) = tmp;
    narr = linked(el, tmp);
}

*-- pop predicate
- relation between whole array, 
popped array, popped index 
and popped element.
`int[] arr = [1,2,3,4,5],`
`pop(arr, arr2, 3) = x`
! will move it lib as it is fully-logical
--*
: ?pop('T'[], 'T'[], index_t) -> 'T'.
?pop(with, without, idx) -> element {
    with[idx] = element;
    without = excluded_idx(with, idx);
}

*--
Determines array to sorted
```
stupid_sorted{int}([2,5,1]) = x
```
--*
: ?stupid_sorted('T'[]) -> 'T'[].
?stupid_sorted([]) -> [].
?stupid_sorted(arr) -> srtd {
    max_w_tail(arr, tail) = m;
    srtd = linked(m, stupid_sorted(tail));
}