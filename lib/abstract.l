-- Abstract nesting of type
: ##abstract_nesting('T') -> 'T'[].

-- Abstract primitive of type
: ##abstract_primitive('T'[]) -> 'T'.

-- Translator signature constraint
: ##translator('T', 'R', (?('T') -> 'R')).

-- Translator reverse signature constraint
: ##rev_translator((?('T') -> 'R')) -> (?('R') -> 'T').