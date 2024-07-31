# Colour Picker

A basic colour picker written in C and [raylib][].

## Requirements
* C11 Compiler
* [raylib][] - Optional, a static version will be built if `USE_SYSTEM_RAYLIB`
  is not set in the environment.

## Installation

Run `build.sh` and copy `colourpicker` where you want it.

## Debug Hot Reloading

If `DEBUG` is set in the environment then the hot reloading
version will be compiled. The program will load
`libcolourpicker.so` at runtime and reload it when it is updated.
In this case raylib **MUST** be dynamically linked.

[raylib]: https://www.raylib.com/
