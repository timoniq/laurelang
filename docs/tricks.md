# laurelang useful tricks

## negation

use implication (negation as failure principle)

```
// pred() must fail
pred() -> fail();
```

## inplace save state

use declaration, reverse it

```
x = between(1, 5);
y x = 3;
message(format("x={x}, y={y}"));
// x=3, y=<1..5>
```