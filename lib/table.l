!use 
    <datetime>,
    <abstract>,
    <string>.

Id ~ natural.
Value ~ union{int, string}.
NullableValue ~ union{Value, Nothing}.
FieldType ~ @{Int8, Int32, String}.

$Field {
    FieldType type;
    string field_name;
    boolean nullable;
}

-- Relation between table name and its fields
: ?table(string, Field[]).

?table(
    "users",
    [
        (@Int32, "id", @false), 
        (@String, "username", @false), 
        (@String, "password_hash", @false)
    ]
).

-- Relation between field and its SQL scheme
: ?field_scheme(Field) -> string.
?field_scheme((type, name, @true)) -> x {
    scheme = field_scheme((type, name, @false));
    x = format("NULLABLE {scheme}");
}%
?field_scheme((@Int32, name, _)) -> format("INTEGER {name}").
?field_scheme((@String, name, _)) -> format("VARCHAR {name}").

: ?fields_scheme(Field[]) -> string[].
?fields_scheme(fields) -> map(fields, field_scheme).

-- Relation between native value field and its raw value
: ?field_from_int(int, Field) -> string.
?field_from_int(x, (@Int8, _, _)) -> repr(x).
?field_from_int(x, (@Int32, _, _)) -> repr(x).

converter ~ (?(_, Field) -> string).

: ?User(Id, string, string).
?User(id, username, password_hash) {
    table("users", fields);
    not_implemented();
}