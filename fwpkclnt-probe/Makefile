CC = x86_64-w64-mingw32-gcc
CFLAGS = -O2 -Wall -Wextra -Werror -fno-stack-protector -fno-builtin

.PHONY: all clean
all: build/fwpkclnt.sys build/probe_test.exe

build:
	mkdir -p build

build/fwpkclnt.sys: fwpkclnt.c fwpkclnt.def | build
	$(CC) $(CFLAGS) -shared -nostdlib -Wl,--entry,DllMain -Wl,--subsystem,native -Wl,--out-implib,build/libfwpkclnt.a -o $@ fwpkclnt.c fwpkclnt.def -lntoskrnl

build/probe_test.exe: probe_test.c build/fwpkclnt.sys
	$(CC) $(CFLAGS) -o $@ probe_test.c -Lbuild -lfwpkclnt

clean:
	rm -rf build
