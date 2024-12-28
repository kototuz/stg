all: build/libraylib.a build/tdlib/Makefile build/td_static
	cmake -B build/td_static -S .
	make -C build/td_static
	mv build/td_static/stg build/

build/libraylib.a:
	make -C raylib/src RAYLIB_RELEASE_PATH=../../build

build/tdlib/libtdapi.a: td build build/tdlib
	su -l
	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX:PATH=. -B build/tdlib -S td
	cmake --build build/tdlib --target install -j 4
	exit

build/td_static:
	mkdir -p $@

build:
	mkdir -p $@

build/tdlib:
	mkdir -p $@
