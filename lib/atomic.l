: ?subset('T') -> 'T'.
?subset(x) -> y when
    x >=@ y,
    x ~ y.

-- both become an intersection of each other
: ?intersect('T') -> 'T'.
?intersect(x) -> x.