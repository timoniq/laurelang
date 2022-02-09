# CLE (C logic enviroment)

Environment is essential for logic programming because we live in world full of procedural technologies and we have to make bridges for procedure driven events to be reasoned by our logic machine.

Laurelang provides an API for these needs, API documentation can be found [click](/docs/cle/index.md).

# Choice point

Choice point is created each time predicate is called, but it also can be created via choice operation. Choice operation consists of different possible expression variations divided by `|` symbol, it also can be applied to other operations recursively.

```laurelang
{
// a is 1, 2 or 1 + 2
a = 1|2|1 + 2;
a + b = 5;
}
```

Pairs for (a, b) will be found:
```
a = 1, b = 4;
a = 2, b = 3;
a = 3, b = 2.
```

> Operations which **do not** create a choice-point are imaging and calling a constraint