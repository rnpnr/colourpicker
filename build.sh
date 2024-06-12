#!/bin/sh

cflags="-march=native -ggdb -O3 -Wall"
ldflags="-lraylib"

# Hot Reloading/Debugging
#cflags="$cflags -D_DEBUG"

libcflags="$cflags -fPIC -flto"
libldflags="$ldflags -shared"

cc $libcflags colourpicker.c -o libcolourpicker.so $libldflags
cc $cflags -o colourpicker main.c $ldflags
