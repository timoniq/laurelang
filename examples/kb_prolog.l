: ?loves(string, string).

?loves("vincent", "mia").
?loves("marcellus", "mia").
?loves("pumpkin", "honey bunny").
?loves("honey bunny", "pumpkin").

: ?jealous(string, string).
?jealous(X, Y) {
    loves(X, Z);
    loves(Y, Z);
    X never Y;
}

: ?mutual(string, string).
?mutual(a, b) {
    loves(a, b);
    loves(b, a);
}