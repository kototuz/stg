CC=g++
CFLAGS=-Wall -Wextra -ggdb
SRC=$(wildcard src/*.cpp)
TD_LIBS=build/libtdapi.a          \
        build/libtdclient.a       \
        build/libtdcore.a         \
        build/libtddb.a           \
        build/libtdjson_private.a \
        build/libtdjson_static.a  \
        build/libtdmtproto.a      \
        build/libtdnet.a          \
        build/libtdsqlite.a       \
        build/libtdutils.a        \
        build/libtdactor.a

all: build/libtdclient.a build/libraylib.a
	$(CC) $(CFLAGS) -o build/stg $(SRC) -Iinclude -Lbuild $(basename $(subst build/lib, -l, $(TD_LIBS))) -lraylib -lm -lz -lssl -lcrypto 

build/libraylib.a: build
	make -C raylib/src RAYLIB_RELEASE_PATH=../../build

build/libtdclient.a: build
	mkdir -p build/tmp
	sudo cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX:PATH=. -B build/tmp -S td
	sudo cmake --build build/tmp --target install -j 4
	sudo mv build/tmp/lib/*.a build/
	sudo rm -rf build/tmp

build:
	mkdir -p $@
