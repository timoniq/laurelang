# Installation

## Building from source

Firstly, clone the repository:

```
git clone https://github.com/timoniq/laurelang.git
cd laurelang
```

Secondly, choose appropriate build setup:

* Auto builder, just run `make auto`. All settings will be set by default. Recommended for newcomers.
* Build assistant, run `python3 build.py`.
* Manual build.

After building sources you may need to run tests:

```
make test
```

## Troubleshooting

`readline` may be not installed on your machine, if it so, install it and rebuild.