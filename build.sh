#!/bin/sh

cflags="-march=native -std=c11 -O3 -Wall"
ldflags="-lraylib -lm"
debug=${DEBUG}

cc=${CC:-cc}
system_raylib=${USE_SYSTEM_RAYLIB:-$debug}

case $(uname -s) in
MINGW64*) ldflags="$ldflags -lgdi32 -lwinmm" ;;
esac

# NOTE: clones and builds a static raylib if system lib is not requested
# NOTE: this requires cmake
if [ "$system_raylib" ]; then
	ldflags="-L/usr/local/lib $ldflags"
else
	if  [ ! -f external/lib/libraylib.a ]; then
		git submodule update --init --depth=1 external/raylib
		cmake --install-prefix="${PWD}/external" \
		      -G "Ninja" -B external/raylib/build -S external/raylib \
		      -D CMAKE_INSTALL_LIBDIR=lib -D CMAKE_BUILD_TYPE="Release" \
		      -D CUSTOMIZE_BUILD=ON -D WITH_PIC=ON -D BUILD_EXAMPLES=OFF
		cmake --build   external/raylib/build
		cmake --install external/raylib/build
	fi
	cflags="$cflags -I./external/include"
	ldflags="-L./external/lib $ldflags"
fi

[ ! -s "config.h" ] && cp config.def.h config.h

if [ ! -e "shader_inc.h" ] || [ "hsv_lerp.glsl" -nt "shader_inc.h" ]; then
	${cc} $cflags -o gen_incs gen_incs.c $ldflags -flto -s
	./gen_incs
fi

if [ "$debug" ]; then
	# Hot Reloading/Debugging
	cflags="$cflags -O0 -ggdb -D_DEBUG -Wno-unused-function"
	# NOTE: needed for sync(3p)
	cflags="$cflags -D_XOPEN_SOURCE=600"

	libcflags="$cflags -fPIC"
	libldflags="$ldflags -shared"

	${cc} $libcflags colourpicker.c -o libcolourpicker.so $libldflags
fi

${cc} $cflags -o colourpicker main.c $ldflags
