# laurelang

A pure logical, compact language.

Work in progress.

# What this language is for?

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

Also you may need to run tests with `make test`. Useful settings and tips for installation [may be found here](/docs/installation.md).

# Documentation

[Tutorial and dev notes](/docs/index.md)  
[Read documentation](https://docs.laurelang.org)

# [Contributing](/contributing.md)
# License

Laurelang is [MIT licensed](/LICENSE)  
Copyright © 2022 [timoniq](https://github.com/timoniq)