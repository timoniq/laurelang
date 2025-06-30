int natural = 1...
int natural* = 0...

index_t ~ natural*.

-- locks
!lock natural.
!lock natural*.
!lock index_t.

*-- Read docs at /docs/builtin.md#integer-add
```a + b = c;
+(a, b) = c;``` 
--*
: ?+(int, int) -> int.

*-- Read docs at /docs/builtin.md#integer-subtract
```a - b = c;
-(a, b) = c;```
--*
: ?-(int, int) -> int.

-- Multiplication predicate
: ?*(int, int) -> int.

-- Integer division predicate
: ?/(int, int) -> int.

: 
    #>(int, int),
    #<(int, int),
    #>=(int, int),
    #<=(int, int).

#>(x, y) {__>(x, y)}
#<(x, y) {__>(y, x)}
#>=(x, y) when x __gte y.
#<=(x, y) when y >= x.

: 
    #>@('T', 'T'),
    #<@('T', 'T'),
    #>=@('T', 'T'),
    #<=@('T', 'T').

#>@(x, y) {__>(x, y)}
#<@(x, y) {__>(y, x)}
#>=@(x, y) {__gte(x, y)}
#<=@(x, y) {__gte(y, x)}

?+(x, y) -> x __+ y.
?-(y + z, y) -> z.
?*(x, y) -> x __* y.
?/(x * y, y) -> x {
    y = 0 -> fail();
}

-- math `sqrt` predicate
: ?sqrt(int) -> int.
?sqrt(0) -> 0.
?sqrt(1) -> 1.
?sqrt(x) -> __sqrt(x).

-- Plus-minus predicate.
-- ```x = (-b +- sqrt(D)) / (2 * a)```
: ?+-(int, int) -> int.

?+-(x, y) -> x + y.
?+-(x, y) -> x - y.

-- Integer pow predicate
!ordering.
: ?pow(int, int) -> int.
?pow(x, 0) -> 1.
?pow(x, 1) -> x.

?pow(Value, Pow) -> R {
    Pow > 1;
    Value?;
    R = Value * pow(Value, Pow - 1);
}

-- Absolute value predicate
-- `absolute of -5 = 5`
: ?absolute(int) -> int.
?absolute(x) -> x when x __gte 0.
?absolute(x) -> -x when x < 0.

-- Integer pow predicate (shortcut to `pow`)
: ?^(int, int) -> int.
?^(v, p) -> pow(v, p).

-- Always failing predicate
: ?fail().

-- Simple constraint
: #never('T', 'T').
#never(a, b) {
    a = b -> fail();
}

-- Message predicate working with stdio
: ?message(string).
?message(m) when __message(m).

-- between (boundaries secluded)
: #between(int, int) -> int.
#between(a, b) -> c when c > a, c < b.

*-- between (boundaries included).
`x = 0 .. 4`
`5 = 0 .. 4 -> fail()` --*
: #..(int, int) -> int.
#..(a, b) -> c when c >= a, c <= b.

cons ~ never.

: ?format(formatting) -> string.
?format(fmt) -> __format(fmt).

: ?-->('T', 'T'[]).
?-->(source, container) when __bag(source, container).

: ?<--('T'[], 'T').
?<--(container, source) when __bag(source, container).

: ?bagof('T') -> 'T'[].
?bagof(source) -> container when __bag(source, container).

: ?sized_bagof(natural, 'T') -> 'T'[].
?sized_bagof(size, source) -> container when
    __bag_sz(source, container, size).

-- working with arrays
: ?by_idx('T'[], int) -> 'T',
  ?append('T'[], 'T'[]) -> 'T'[].

?by_idx(arr, idx) -> __by_idx(arr, idx).
?append(arr, tail) -> __append(arr, tail).

-- predicate states that there are no 
-- nearby standing equal elements in array
: ?no_repeat('T'[]).

?no_repeat([]).
?no_repeat(arr) when length of arr = 1, arr?.

?no_repeat(arr) {
    i > 0;
    len = length of arr;
    len > 1;
    arr?;
    i < len;
    &all i {
        arr[i] never arr[i - 1];
    };
}

: ?all_distinct('T'[]).
?all_distinct([]).
?all_distinct([head|tail]) {
    head in tail -> fail();
    all_distinct(tail);
}

*-- links array head to array tail
`linked{int}(1, [2,3]) = x`
`linked{int}(1, tail) = [1,2,3]` --*
: ?linked('T', 'T'[]) -> 'T'[].

?linked(head, tail) -> arr {
    arr[0] = head;
    append([head], tail) = arr;
}

!ordering.
: ?linkedr('T'[], 'T') -> 'T'[].
?linkedr(tail, head) -> arr {
    append(tail, [head]) = arr;
    arr[length(arr) - 1] = head;
}

-- flattens array to one dimension
-- `flatten{int[]}([[1,2], [], [3], [4, 5]]) = x`
!ordering.
: ?flatten('T'[]) -> 'T'.

?flatten([]) -> [].

?flatten(array) -> flattened {
    linked(head, tail) = array;
    flattened = head:append(tail:flatten());
}

: ?in('T', 'T'[]).
?in(x, arr) when
    arr[_] = x.

__rounding ~ @{down, up}.

: ?sqrtu(int) -> int.
?sqrtu(n) -> x {
    __rounding r = @up;
    x = __sqrt_round(n, r);
}

: ?sqrtd(int) -> int.
?sqrtd(n) -> x {
    __rounding r = @down;
    x = __sqrt_round(n, r);
}

: ?map('T'[], (?('T') -> 'R')) -> 'R'[].
?map(seq, mapping) -> mseq {
    __map(seq, mapping) = mseq;
}

$ divmod_t {
    int div;
    int mod;
}

-- `X / Y = R + Mod`
: ?divmod(int, int) -> divmod_t.
?divmod(x, y) -> dm {
    __*mod(a, y, r) = x;
    dm = (a, r);
}

: ?/_(int, int) -> int.
?/_(x, y) -> z {
    divmod(x, y) = dm;
    dm = (z, _);
}

: ?from('T'[], index_t) -> 'T'[].
?from(arr, 0) -> arr.
?from(arr, n) -> tail {
    n = ..(1, length(arr))?;
    s ~ T[n];
    append(s, tail) = arr;
}

: ?to('T'[], index_t) -> 'T'[].
?to(arr, 0) -> [].
?to(arr, n) -> tail {
    n = ..(1, length(arr))?;
    s ~ T[n];
    append(s, _) = arr;
    tail = s;
}

: ?slice('T'[], index_t, index_t) -> 'T'[].
?slice(arr, f, t) -> s {
    s = (arr from f) to (t - f);
}

: ?max_of_two(int, int) -> int.
?max_of_two(a, b) -> a when a >= b.
?max_of_two(a, b) -> b when b > a.

: ?min_of_two(int, int) -> int.
?min_of_two(a, b) -> a when max_of_two(a, b) = b.
?min_of_two(a, b) -> b when max_of_two(a, b) = a.

: ?choose_one('T'[], (?('T', 'T') -> 'T')) -> 'T'.
?choose_one(arr, _) -> arr[0] when 
    length(arr) = 1.
?choose_one(arr, choose) -> choose(arr[0], arr[1]) when 
    length(arr) = 2.
?choose_one(arr, choose) -> x {
    a = arr[0];
    b = arr[1];
    t = from(arr, 2);
    x = choose(choose(a, b), choose_one(t, choose));
}

-- array to its maximum value
: ?max(int[]) -> int.
?max(arr) -> choose_one(arr, max_of_two).

-- array to its minumum value
: ?min(int[]) -> int.
?min(arr) -> choose_one(arr, min_of_two).

boolean ~ @{false, true}.
Nothing ~ @{Nothing}.

*--
`at` predicate is a relation between 
1-infix arity predicate, argument and resulting argument
Example for predicate search:
```
pred at{int} -10 = 10
-- `pred` is instantiated to `absolute`
```
--*
: ?at((?('T') -> 'R'), 'T') -> 'R'.
?at(pred, x) -> pred(x).

: ?not_implemented().
?not_implemented() {
    !error "Not implemented";
}

-- Determines array to predicate and filtered array 
-- with elements corresponding to predicate
: ?filter('T'[], (?('T'))) -> 'T'[].
?filter([], _) -> [].
?filter([x|tail], f) -> fs {
    f(x);
    append([x], filter(tail, f)) = fs;
}%
?filter([_|tail], f) -> filter(tail, f).

: ?reverse_predicate((?('T') -> 'R')) -> (?('R') -> 'T').
?reverse_predicate(pred) -> rev_pred {
    -- constructor
    not_implemented();
}

: ?incr(int) -> int.
?incr(x) -> x + 1.

: ?decr(int) -> int.
?decr(x) -> x - 1.

Image ~ @{int, char, array}.

: ?image('T') -> Image.
?image(int) -> @int.
?image(char) -> @char.
?image(x) -> @array {
    x ~ _[];
}

-- Determines 1-arity predicate with its solution
-- to boolean considering solution validity
-- `as_boolean(prime, x) = @false` - to declare x as non-prime number
: ?as_boolean((?('T')), 'T') -> boolean.
?as_boolean(f, arg) -> b {
    b = @true;
    f(arg);
}
?as_boolean(f, arg) -> b {
    b = @false;
    arg?;
    f(arg) -> fail();
}

: ?functor((?('T', 'R')), 'T', 'R') -> boolean.
?functor(f, x, y) -> b {
    b = @true;
    f(x, y);
}
?functor(f, x, y) -> b {
    b = @false;
    f(x, y) -> fail();
}

: #instantiated(_).

#instantiated(x) { 
    message("internal wrapper");
    __instantiated(x);
}
