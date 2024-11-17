natural* day_delta_t = 0..31.
natural* month_delta_t = 0..12.

day_delta_t day_t = 1..31.
month_delta_t month_t = 1..12.

natural second_t = 0..59.
minute_t ~ second_t.
natural hour_t = 0..23.

day_t[] month_days = [
    31, 28, 31, 30, 31, 30,
    31, 31, 30, 31, 30, 31
].

$date {
    day_t day;
    month_t month;
    int year;
}

$time {
    hour_t hours;
    minute_t minutes;
    second_t seconds;
}

$datetime {
    date d;
    time t;
}

$datedelta {
    day_delta_t day_delta;
    month_delta_t month_delta;
    int year_delta;
}

: ?leap_year(int).
?leap_year(x) {
    x / 4;
    x / 100 -> fail();
}%
?leap_year(x) {
    x / 400;
}

: ?leap_years(int) -> int.
?leap_years(0) -> _.
?leap_years(y) -> ly {
    ly = (y /_ 4) - (y /_ 100) + (y /_ 400);
}

: ?month_year_days(natural*) -> natural*.
?month_year_days(0) -> 0.
?month_year_days(n) -> x {
    i = n - 1;
    ds = month_days[i];
    my = month_year_days(i);
    x = my + ds;
}

: ?in_days(date) -> natural*.
?in_days((d, m, y)) -> 
    (y * 365) + month_year_days(m - 1) + leap_years(y) + d.

: ?year_days(int) -> natural*.
?year_days(x) -> 365 {
    leap_year(x) -> fail();
}
?year_days(x) -> 366 {
    leap_year(x);
}

: ?as_delta(natural*, date) -> datedelta.
?as_delta(days, start) -> dt {
    days >= 365;
    start = (start_d, start_m, y);
    yn = y + 1;
    dt = as_delta(days - year_days(y), (start_d, start_m, yn));
}
?as_delta(days, start) -> dt {
    days < 365;
    start = (start_d, m, start_y);
    md = month_days[m - 1];
    days >= md;
    idx = m + 1;
    date ndate = (start_d, idx, start_y);
    dt = as_delta(days - md, ndate);
}
?as_delta(days, start) -> dt {
    start = (start_d, m, start_y);
    int idx = m - 1;
    days <= month_days[idx];
    ds = days + start_d - 1;
    dt = (ds, idx, start_y);
}

: ?date_subtract(date, date) -> datedelta.
?date_subtract(x, y) -> z {
    delta = in_days(x) - in_days(y);
    as_delta(delta, (1, 1, 0)) = z;
}