: ?range(int, int) -> int[].
?range(start, to) -> sized_bagof(to - start, (start .. to)?).

: ?step(int, int) -> int.
?step(a, s) -> a + s.
?step(a, s) -> step(a + s, s).

: ?step_included(int, int) -> int.
?step_included(a, _) -> a.
?step_included(a, s) -> step(a, s).

: ?range_step(int, int) -> int[].
?range_step(start, s) -> bagof(step_included(start, s)).