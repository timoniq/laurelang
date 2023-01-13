!use
    std:{
        arrays, 
        abstract
    },
    examples:{
        with_parameters
    }.

: ?predicate(int[]) -> int[].
?predicate(xs) -> map(xs, parametrized).
