<p align="center">
<img src="https://raw.githubusercontent.com/timoniq/laurelang/main/.github/logo.svg" height="150px" align="center">
</p>
<h1 align="center">laurelang</h1>
<p align="center"><b>A pure logical, compact language.</b></p>
<p align="center">Work in progress.</p>
<hr>

# Usage

Laurelang stands for Logic, Abstraction and Unification with Readability and Efficience.

Language aims to become reasonable choice for declarative, logic, constraint programming research, performant for quering and reasoning in big data.

Sample prime number predicate declaration:

```laurelang
: ?prime(natural).

?prime(1).
?prime(2).
?prime(n) {
    n > 2; n?;
    b = 2 .. sqrtu(n);
    &all b {
        n / b ->
            fail();
    };
}
```

Further reasoning on this predicate:

```laurelang
?- prime(11)
   true
?- prime(20..22)
   false
?- prime(x)
   x = 1; x = 2; x = 3; 
   x = 5; x = 7; x = 11...
?- prime(x), sized_bagof(15, x) = y
   x = 43,
   y = [1, 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43].
```

Using predicate `absolute` as mapper:

```laurelang
?- x = map{int}([1, -2, -3], absolute)
   x = [1, 2, 3].
?- [1, 2] = map{int}(x, absolute)
   x = [1, 2];
   x = [1, -2];
   x = [-1, 2];
   x = [-1, -2].
```

# Getting started

## Build from source

Clone the repository and run auto-builder:

```
git clone https://github.com/timoniq/laurelang.git
cd laurelang
make auto
```

Auto-builder will also run test suite.

# Documentation

[Read documentation](https://docs.laurelang.org)  
[Recent updates and notes](/docs/index.md)

# [Contributing](https://laurelang.org/contrib)
# License

[MIT license](/LICENSE)  
Copyright © 2022 [timoniq](https://github.com/timoniq)