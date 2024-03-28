!use std:arrays.

: ?palindrome_n(natural) -> int.
?palindrome_n(n) -> (((n + 1) * n) /_ 2).

: ?isPalindrome('T'[]).
?isPalindrome(xs) {
    xs = reverse(xs);
}

: ?subarys('T'[]) -> 'T'[][].
?subarys(xs) -> (
    palindrome_n(length(xs)) sized_bagof subarray(xs)
).

: ?maxPalindrome('T'[]) -> 'T'[].
?maxPalindrome(xs) -> palindrome {
    palindrome = longest(filter(subarys(xs), isPalindrome));
}
