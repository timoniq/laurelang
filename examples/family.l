!use std:datetime.

date today = (28, 01, 2024).
gender ~ @{male, female}.

: ?person(string, gender, date) -> ID,
  ?parent(person) -> person,
  ?age(person) -> natural*.

?person("John", @male, (12, 04, 1995)).
?person("Annie", @female, (23, 01, 1962)).
?person("Oscar", @male, (02, 12, 2000)).
?person("Alexander", @male, (05, 10, 2001)).
?person("Joseph", @male, (13, 06, 1952)).
?person("Marine", @female, (28, 02, 1958)).
?person("Ellen", @female, (19, 11, 1976)).
?person("Tatiana", @female, (20, 06, 1980)).
?person("Igor", @male, (12, 12, 1975)).
?person("Olga", @female, (01, 02, 2003)).
?person("Oleg", @male, (04, 05, 2002)).
?person("Selma", @female, (18, 03, 2000)).
?person("Akiv", @female, (27, 05, 2005)).

?parent(person("Joseph", _, _), person("Ellen", _, _)).
?parent(person("Marine", _, _), person("Ellen", _, _)).
?parent(person("Igor", _, _), person("Selma", _, _)).
?parent(person("Ellen", _, _), person("Selma", _, _)).
?parent(person("Igor", _, _), person("Alexander", _, _)).
?parent(person("Ellen", _, _), person("Alexander", _, _)).
?parent(person("Igor", _, _), person("Olga", _, _)).
?parent(person("Igor", _, _), person("Oleg", _, _)).
?parent(person("Tatiana", _, _), person("Olga", _, _)).
?parent(person("Tatiana", _, _), person("Oleg", _, _)).
?parent(person("Annie", _, _), person("John", _, _)).
?parent(person("Annie", _, _), person("Oscar", _, _)).
?parent(person("Joseph", _, _), person("John", _, _)).
?parent(person("Joseph", _, _), person("Oscar", _, _)).
?parent(person("Ellen", _, _), person("Akiv", _, _)).

?age(person(_, _, dob)) -> years {
    delta = date_subtract(today, dob);
    delta = (_, _, years);
}

: ?mother(person) -> person.
?mother(x) -> y {
    parent(y, x);
    y = person(_, @female, _);
}

: ?is_full_age(person).
?is_full_age(p) {
    age(p) >= 18;
}