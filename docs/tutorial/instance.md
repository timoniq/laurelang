# Instance

Laurelang stack consists of instances. Each instance must have a state (unified/instantiated or not unified/instantiated). Instantiated instances have a single concrete value specified. Not instantiated instances have typically a domain.

Let's look at `int` for instance. As an example, in instantiated state it may be 42, in not instantiated state: (0, 100] domain. Instances may be ancestors of other instances. Ancestor creation is called imaging. There is imaging operation in laurelang symbolised with `~`, so if we want to create an instance imaged from int (which is just a infinite integer domain in fact), we use:

```laurelang
a ~ int
```

Now a have the same domain as int and int is ancestor of `a`, so if we run this query again we will get true. Also we will get true if we run `a ~ a`, `int ~ int`. Some instances like `int`, `char`, `string` are locked and have a special privilege in logical operations

Instances may also be asserted. Assertion may be treated as a predicate (but not really). We declare that the left side is equal to the right side, and elements are trying to get instantiated. Note:

* Builtin and artificially locked instances are not getting instantiated
* Left element have a previlege for instantiation

```laurelang
a = int
```

This query means that a is equal to some int. So if we run it we will be trying to iterate on infinity which is not possible in standard way (so will get error normally), but we may use some progression to iterate like this: `0 -> 1 -> -1 -> 2 -> -2 -> ...`. To use this generator for int you may set laurelang flag `-int_jmp`.

## Instance components

Instance consists of name, image, representation generator, docstring and translator.  
Image can be derived from these types: integer, array, structure, atom and predicate of two kinds.

Unified integer can store its value in three c-types depending on value itself: `char`, `int` and special big integer handling integers of arbitrary values.