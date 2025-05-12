#!/bin/sh

version=1.0

cflags=${CFLAGS:-"-march=native -O3"}
cflags="${cflags} -std=c11"
ldflags=${LDFLAGS}
# TODO(rnp): embed shader without lto stripping it */
ldflags="${ldflags} -fno-lto -lm"

output="colourpicker"

cc=${CC:-cc}

for arg; do
	case ${arg} in
	debug) debug=1
	esac
done

machine=$(uname -m)
mkdir -p out

case $(uname -s) in
MINGW64*)
	w32=1
	output="Colour Picker"
	windres assets/colourpicker.rc out/colourpicker.rc.o
	ldflags="out/colourpicker.rc.o ${ldflags} -mwindows -lgdi32 -lwinmm"
	case ${machine} in
	x86_64) binary_format="pe-x86-64"     ;;
	*)      binary_format="pe-${machine}" ;;
	esac
	;;
*)
	case ${machine} in
	aarch64) binary_format="elf64-littleaarch64" ;;
	x86_64)  binary_format="elf64-x86-64"  ;;
	esac
	;;
esac

build_raylib()
{
	cp external/raylib/src/raylib.h external/raylib/src/rlgl.h out/
	src=external/raylib/src
	srcs="rcore rglfw rshapes rtext rtextures utils"

	raylib_cmd="${cflags} -Iexternal/raylib/src -Iexternal/raylib/src/external/glfw/include"
	raylib_cmd="${raylib_cmd} -DPLATFORM_DESKTOP_GLFW"
	raylib_cmd="${raylib_cmd} -Wno-unused-but-set-variable -Wno-unused-parameter"
	[ ! ${w32} ] && raylib_cmd="${raylib_cmd} -D_GLFW_X11"

	raylib_ldflags="-lm"
	[ ${w32} ] && raylib_ldflags="${raylib_ldflags} -lgdi32 -lwinmm"

	if [ ${debug} ]; then
		files=""
		for n in ${srcs}; do
			files="${files} ${src}/${n}.c"
		done
		raylib_cmd="${raylib_cmd} -DBUILD_LIBTYPE_SHARED -D_GLFW_BUILD_DLL"
		raylib_cmd="${raylib_cmd} -fPIC -shared"
		raylib_cmd="${raylib_cmd} ${files} -o ${raylib}"
		${cc} ${raylib_cmd} ${raylib_ldflags}
	else
		outs=""
		for n in ${srcs}; do
			${cc} ${raylib_cmd} -c "${src}/${n}.c" -o "out/${n}.o" ${raylib_ldflags}
			outs="${outs} out/${n}.o"
		done

		ar rc "${raylib}" ${outs}
	fi
}

if [ $(git diff-index --quiet HEAD -- external/raylib) ]; then
	git submodule update --init --checkout --depth=1 external/raylib
fi

[ ! -s "config.h" ] && cp config.def.h config.h

raylib=out/libraylib.a
[ ${debug} ] && raylib="libraylib.so"
[ "./build.sh" -nt ${raylib} ] || [ ! -f ${raylib} ] && build_raylib

cflags="${cflags} -Wall -Wextra -Iout"

if [ ! -s "out/lora_sb_0_inc.h" ] || [ "gen_incs.c" -nt "out/lora_sb_0_inc.h" ]; then
	${cc} ${cflags} -o gen_incs gen_incs.c ${raylib} ${ldflags} && ./gen_incs
fi

if [ "$debug" ]; then
	# Hot Reloading/Debugging
	cflags="${cflags} -O0 -ggdb -D_DEBUG -Wno-unused-function"

	${cc} ${cflags} -fPIC -shared colourpicker.c -o colourpicker.so
	ldflags="${ldflags} -Wl,-rpath,. ${raylib}"
else
	if [ ! -s "out/slider_lerp.o" ] || [ "slider_lerp.glsl" -nt "out/slider_lerp.o" ]; then
		objcopy --input-target=binary slider_lerp.glsl --output-target=${binary_format} out/slider_lerp.o
	fi
	ldflags="${raylib} out/slider_lerp.o ${ldflags}"
fi

${cc} ${cflags} -DVERSION="\"${version}\"" main.c -o "${output}" ${ldflags}
