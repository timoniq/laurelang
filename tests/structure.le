natural day_t = 1..31.
natural month_t = 1..12.

$date {
    day_t day;
    month_t month;
    int year;
}

-- (day=31, month=7, year=2022)
-- (day=25, month=12, year=-6)
-- (day=25, month=12, year=-5)
-- (day=25, month=12, year=-4)
: ?test_structure(date).
?test_structure((31, 07, 2022)).
?test_structure((25, 12, x)) {
    x = (-6 .. -4)?;
}

$ Single {
    string i;
}

: ?test_structure_single_field(string).
?test_structure_single_field(x) {
    Single a = "hello world";
    a = (x,);
}

?test_structure_single_field(x) {
    Single a = ("hello world",);
    a = (x,);
}