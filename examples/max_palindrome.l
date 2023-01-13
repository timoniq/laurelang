!use std:arrays.

: ?palindrome_n(natural) -> int.
?palindrome_n(n) -> (((n + 1) * n) /_ 2).

: ?isPalindrome(int[]).
?isPalindrome(xs) {
    xs = reverse(xs);
}

: ?subarys(int[]) -> int[][].
?subarys(xs) -> (
    palindrome_n(length(xs)) sized_bagof subarray(xs)
).

: ?maxPalindrome(int[]) -> int[].
?maxPalindrome(xs) -> palindrome {
    palindrome = longest(filter(subarys(xs), isPalindrome));
}%
