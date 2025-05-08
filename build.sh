#!/bin/sh

version=1.0

cflags=${CFLAGS:-"-march=native -O3"}
cflags="${cflags} -std=c11 "
ldflags=${LDFLAGS:-"-flto"}
ldflags="$ldflags -lm"

output="colourpicker"

cc=${CC:-cc}

for arg; do
	case ${arg} in
	debug) debug=1
	esac
done

case $(uname -s) in
MINGW64*)
	w32=1
	output="Colour Picker"
	windres assets/colourpicker.rc out/colourpicker.rc.o
	ldflags="out/colourpicker.rc.o ${ldflags} -mwindows -lgdi32 -lwinmm"
	;;
esac

build_raylib()
{
	cp external/raylib/src/raylib.h external/raylib/src/rlgl.h out/
	src=external/raylib/src
	srcs="rcore rglfw rshapes rtext rtextures utils"

	raylib_cmd="${cflags} -I./external/raylib/src -DPLATFORM_DESKTOP_GLFW"
	raylib_cmd="${raylib_cmd} -Wno-unused-but-set-variable -Wno-unused-parameter"
	[ ! ${w32} ] && raylib_cmd="${raylib_cmd} -D_GLFW_X11"

	if [ ${debug} ]; then
		files=""
		for n in ${srcs}; do
			files="${files} ${src}/${n}.c"
		done
		raylib_cmd="${raylib_cmd} -DBUILD_LIBTYPE_SHARED -D_GLFW_BUILD_DLL"
		raylib_cmd="${raylib_cmd} -fPIC -shared"
		raylib_cmd="${raylib_cmd} ${files} -o ${raylib}"
		[ ${w32} ] && raylib_cmd="${raylib_command} -L. -lgdi32 -lwinmm"
		${cc} ${raylib_cmd}
	else
		outs=""
		for n in ${srcs}; do
			${cc} ${raylib_cmd} -c "${src}/${n}.c" -o "out/${n}.o"
			outs="${outs} out/${n}.o"
		done

		ar rc "${raylib}" ${outs}
	fi
}

if [ $(git diff-index --quiet HEAD -- external/raylib) ]; then
	git submodule update --init --checkout --depth=1 external/raylib
fi

[ ! -s "config.h" ] && cp config.def.h config.h

mkdir -p out
raylib=out/libraylib.a
[ ${debug} ] && raylib="libraylib.so"
[ "./build.sh" -nt ${raylib} ] || [ ! -f ${raylib} ] && build_raylib

cflags="${cflags} -Wall -Wextra -Iout"

if [ ! -e "out/shader_inc.h" ] || [ "slider_lerp.glsl" -nt "out/shader_inc.h" ]; then
	${cc} ${cflags} -o gen_incs gen_incs.c ${ldflags} ${raylib}
	./gen_incs
	mv lora_sb*.h out/
fi

if [ "$debug" ]; then
	# Hot Reloading/Debugging
	cflags="${cflags} -O0 -ggdb -D_DEBUG -Wno-unused-function"

	${cc} ${cflags} -fPIC -shared colourpicker.c -o colourpicker.so
	ldflags="${ldflags} -Wl,-rpath,. ${raylib}"
else
	ldflags="${raylib} ${ldflags}"
fi

${cc} ${cflags} -DVERSION="\"${version}\"" main.c -o "${output}" ${ldflags}
