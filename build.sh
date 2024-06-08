#!/bin/sh

cflags="-march=native -O2 -Wall"
ldflags="-lraylib"

libcflags="-march=native -Wall -O2 -fPIC"
libldflags="$ldflags -shared"

cc $libcflags colourpicker.c -o libcolourpicker.so $libldflags
cc $cflags -o colourpicker main.c $ldflags
