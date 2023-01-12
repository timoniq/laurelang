: ?test_atom().
?test_atom() {
    A ~ @{pen, ruler, compass};
    A = @phone -> fail();
    &exists A { A = ruler };
    &exists A { A = pen };
    &exists A { A = compass };
}

: ?test_atom_intersect().
?test_atom_intersect() {
    A ~ @{a, b, c};
    A = @{c, a, b};
    B ~ @{p, q, c};
    A = B;
    &all A {A = @{c}; A = B};
    A = @{c, c};
}

-- "@{H, O}"
-- "@{хорек, мышь}"
: ?test_atom_distinct(string).
?test_atom_distinct(r) {
    A ~ @{H, H, O};
    r = repr(A);
}
?test_atom_distinct(r) {
    animals ~ @{хорек, мышь, хорек, мышь};
    r = repr(animals);
}

-- @atom1
-- @atom2
-- @1
-- @X
ATOMS_FOR_TEST ~ @{atom1, "atom2", 1, X}
: ?test_atom_gen(ATOMS_FOR_TEST).
?test_atom_gen(x) when x?.