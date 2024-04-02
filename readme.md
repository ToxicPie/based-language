# Help, what does it mean to be "Based"

Reference implementations of the joke language used in [this Codeforces April Fools problem](https://codeforces.com/contest/1952/problem/J).

## Rust Checker

This is the current version used on Codeforces. The one used during contest was the C++ one below. It was replaced later with a Rust rewrite due to some implementation issues.

Compile with

```
rustc -O checker.rs
```

and run with

```
./checker <input_file> <output_file> <answer_file>
```

## C++ Checker

Compile with

```
g++ -O2 -o checker checker.cpp
```

and run with

```
./checker <type> < <program_file>
```

where `type` is the problem ID in the Codeforces problem.

## C++ Interpreter

First modify the input/outputs by editing the code according to your needs. Then compile with

```
g++ -O2 -o interpreter interpreter.cpp
```

and run with

```
./interpreter < <program_file>
```
