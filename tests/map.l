
-- [1, 0, 3]
-- [2, 10]
: ?test_map(int[]).

-- [1, 2]
-- [1, -2]
-- [-1, 2]
-- [-1, -2]
: ?test_map_back(int[]).

?test_map(map{int}([-1, 0, -3], absolute)).
?test_map(map{int}([-2, 10], absolute)).

?test_map_back(x) {
    map{int}(x, absolute) = [1, 2];
}