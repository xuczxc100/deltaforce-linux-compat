CC = gcc
WINCC = x86_64-w64-mingw32-gcc
CFLAGS = -O2 -Wall -Wextra -Werror

.PHONY: all overlay
all: build/window_band.o build/band_test.exe

build:
	mkdir -p build

build/window_band.o: window_band.c | build
	$(CC) $(CFLAGS) -fPIC -fno-stack-protector -fno-builtin -fno-omit-frame-pointer -c $< -o $@

build/band_test.exe: band_test.c raw_syscall.S | build
	$(WINCC) $(CFLAGS) $^ -o $@ -luser32

overlay: all
	test -n "$(WIN32U_SOURCE)"
	python3 overlay.py "$(WIN32U_SOURCE)" build/win32u.so
