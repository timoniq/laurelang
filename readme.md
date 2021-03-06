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

```
: ?prime(natural).

?prime(1).
?prime(2).
?prime(n) {
    n > 2; n?;
    b = between(1, n);
    &all b {
        n / b ->
            fail();
    };
}
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

Laurelang is [MIT licensed](/LICENSE)  
Copyright © 2022 [timoniq](https://github.com/timoniq)