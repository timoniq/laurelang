# Package

## What package consists of

Package may be single-file or a folder with init file inside it.

Init file should be named same as package folder, extension is `.le`.

```
my_package
   - my_package.le
   - first_tool.le
   - second_tool.le
```

Usually header file just include other public package resources.

## How to include

Use command `use`:

```
!use "my_package", "@/http"
```

There can be any number of packages to include declared.
The `@` symbol means that package is the global package (such as std).

## How to include CLE packages

Use command `useso`.