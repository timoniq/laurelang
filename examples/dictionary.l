$ kv {
    'K' key;
    'V' value;
}


dict{'K', 'V'} ~ kv{'K', 'V'}[].

?values(dict{'K', 'V'}) -> 'V'[] {
    ([]) -> [];
    (dct) -> vs {
        dct = [first|tail];
        vs = append([first:value], :(tail));
    }
}

?keys(dict{'K', 'V'}) -> 'K'[] {
    (dct) -> vs {
        dct = [first|tail];
        vs = append([first:key], :(tail));
    }
}

?backward(dict{'K', 'V'}) -> dict{'V', 'K'} {
    (dct) -> bck {
        bck[i] = (dct[i]:value, dct[i]:key);
    }
}

?get(dict{'K', 'V'}, 'K') -> 'V' {
    (dct, key) -> value {
        &e dct[i] {
            dct[i] = (key, value);
        }
    }
}

?pop(dict{'K', 'V'}, 'K', dict{'K', 'V'}) -> 'V' {
    ([first|tail], key, exclude_key(tail, key, value)) -> value {
        first:key = key;
        value = first:value;
    }
    ([first|tail], key, append([first], exclude_key(tail, key, value))) -> value {
        first:key != key;
    }
}

