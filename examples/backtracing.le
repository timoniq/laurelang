: ?backtraced(int).

: ?is_error().
?is_error() when !error "panic".

?backtraced(1).
?backtraced(2) when is_error().
?backtraced(x) when backtraced(x - 1).