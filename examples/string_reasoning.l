string lowers = "abcdefghijklmnopqrstuvwxyz".
string uppers = "ABCDEFGHIJKLMNOPQRSTUVWXYZ".

: ?lower_letter(char) -> char.
?lower_letter(uppers[i]) -> lowers[i].
?lower_letter(lowers[i]) -> lowers[i].

: ?upper_letter(char) -> char.
?upper_letter(lowers[i]) -> uppers[i].
?upper_letter(uppers[i]) -> uppers[i].

: ?lower_char(char) -> char.
?lower_char(c) -> lower_letter(c)%
?lower_char(x) -> x {
    x in uppers -> fail();
}

: ?upper_char(char) -> char.
?upper_char(c) -> upper_letter(c)%
?upper_char(x) -> x {
    x in lowers -> fail();
}

: ?lower(string) -> string.
?lower(s) -> l {
    map(s, lower_char) = l;
}

: ?upper(string) -> string.
?upper(s) -> u {
    map(s, upper_char) = u;
}