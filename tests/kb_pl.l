-- "vincent";"mia"
-- "marcellus";"mia"
-- "pumpkin";"honey bunny"
-- "honey bunny";"pumpkin"
: ?test_kbpl_loves_all(string, string).
?test_kbpl_loves_all(x, y) {
    loves(x, y);
    loves("vincent", "mia");
    loves("pumpkin", "honey bunny");
    loves("mia", "vincent") -> fail();
}

: ?test_kbpl_loves_self().
?test_kbpl_loves_self() {
     loves(x, x) -> fail();
}

: ?test_kbpl_loves_any().
?test_kbpl_loves_any() {
    loves(_, _);
    loves("marcellus", _);
    loves("mia", _) -> fail();
}

-- "vincent";"marcellus"
-- "marcellus";"vincent"
: ?test_kbpl_jealous(string, string).
?test_kbpl_jealous(var1, var2) {
    #{jealous(x, y) -> jealous(y, x)};
    #{jealous(x, y) -> jealous(x, x) -> fail()};
    jealous(var1, var2);
}

-- "pumpkin";"honey bunny"
-- "honey bunny";"pumpkin"
: ?test_kbpl_mutual(string, string).
?test_kbpl_mutual(x, y) {
    mutual("vincent", "marcellus") -> fail();
    mutual("marcellus", "mia") -> fail();
    mutual("honey bunny", "pumpkin");
    mutual("pumpkin", "honey bunny");
    mutual(x, y);
}