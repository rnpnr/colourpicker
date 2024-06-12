# Colour Picker

A basic colour picker written in C and [raylib][].

## Requirements
* raylib > v5 (Current release has a bug in mouse button code).

## Installation

Run `build.sh` and copy `colourpicker` where you want it.

## Debug Hot Reloading

Add `-D_DEBUG` to the cflags and build. The program will load
`libcolourpicker.so` at runtime and reload it when it is updated.
This is meant solely for development and is not guaranteed to be
working everywhere.

[raylib]: https://www.raylib.com/
