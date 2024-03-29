natural penny_t = 0..99.

$money {
    natural* dollar;
    penny_t penny;
}

$food {
    string name;
    natural calories;
}

$kit {
    food first;
    food second;
}

: ?price(food, money).
?price(("Apple", 50), (0, 50)).
?price(("Milk", 300), (2, 30)).
-- water won't be considered food because it has 0 calories,
-- when calories number should be natural
?price(("Water", 0), (1, 0)).
?price(("Coffee", 1), (2, 10)).

: ?menu(kit, money).
?menu((("Apple", 50), ("Milk", 300)), (2, 0)).
?menu((("Coffee", 1), ("Apple", 20)), (1, 50)).

: ?money_ceiled(natural*) -> money.
?money_ceiled(x) -> (x, 0).
?money_ceiled(x + 1) -> (x, natural).

-- Examples
-- * get apple price
-- ?- price(("Apple", _), price)

: ?pred().
?pred() {
    menu(((first_name, _), (second_name, _)), price);
    price;
    money_ceiled(x) = price;
    message(format("i'll eat {first_name} for first and {second_name} for second and pay {x}"));
}