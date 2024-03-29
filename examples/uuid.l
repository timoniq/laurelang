: ?person(string, natural) -> ID.
?person("Anya", 21).
?person("Nick", 19).
?person("John", 43).
?person("Helena", 19).
?person("Max", 9).

: ?marriage(person, person).
?marriage(person("Anya", _), person("John", _)).
?marriage(person("Nick", _), person("Helena", _)).

: ?predicate2(person) -> natural.
?predicate2(person(_, age)) -> 0 when age < 10.%
?predicate2(person(_, age)) -> age - 10.

: ?statement(string).

?statement(name) {
    age = predicate2(person(name, _));
    age > 0;
    message(format("10 years ago {name} was {age}"));
}%

?statement(name) {
    person(name, _);
    message(format("{name} wasn't even born!"));
}

*--
examples:
* Get 3 or more person's names
?- person(name, _), sized_bagof(x, name) = bag, x >= 3
* Get marriages where age difference between 
  two partners is greater than 5
?- marriage('person(n1, a1), 'person(n2, a2)), 
   absolute(a1 - a2) > 5, 
   bag <-- format("{n1} with {n2}").
--*