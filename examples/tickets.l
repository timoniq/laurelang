!use std:datetime.

: ?user(string, natural) -> ID.

?user("Arseny", 17).
?user("Nicolai", 19).

: ?event(string, date) -> ID.

?event("Ishome Concert", (6, 8, 2022)).

: ?ticket(user, event) -> ID.

?ticket(user("Arseny", _), event("Ishome Concert", _)).
?ticket(user("Nicolai", _), event("Ishome Concert", _)).

: ?ill(user).
?ill(user("Nicolai", _)).

: ?friendship(user, user).
?friendship(user("Arseny", _), user("Nicolai", _)).

: ?friends(user, user).
?friends(x, y) when friendship(x, y).
?friends(x, y) when friendship(y, x).

: ?who_goes(event) -> user.
?who_goes(e) -> u {
    ticket(u, e);
    ill(u) -> fail();
}

: ?is_sad(user).
?is_sad(u) {
    friends(u, x);
    ticket(u, e);
    ticket(x, e);
    ill(x) | ill(u);
}