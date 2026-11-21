CC = x86_64-w64-mingw32-gcc
CFLAGS = -O2 -Wall -Wextra -Werror -fno-stack-protector -fno-builtin

.PHONY: all clean
all: build/regcompat.dll build/query_test.exe build/registry_thunk.bin build/registry_unwind.bin build/basefs.bin build/basefs_test.exe build/icu_test.exe

build:
	mkdir -p build

build/regcompat.dll: regcompat.c regcompat.def | build
	$(CC) $(CFLAGS) -shared -nostdlib -Wl,--entry,DllMain -o $@ regcompat.c regcompat.def -lntdll

build/query_test.exe: query_test.c | build
	$(CC) $(CFLAGS) -o $@ query_test.c -ladvapi32

build/registry_thunk.o: registry_thunk.S | build
	$(CC) -c -o $@ $<

build/registry_thunk.bin: build/registry_thunk.o
	x86_64-w64-mingw32-objcopy -O binary --only-section=.text $< $@

build/registry_unwind.bin: build/registry_thunk.o
	x86_64-w64-mingw32-objcopy -O binary --only-section=.xdata $< $@

build/basefs.o: basefs.c | build
	$(CC) $(CFLAGS) -fmax-errors=3 -fno-asynchronous-unwind-tables -I/usr/x86_64-w64-mingw32/include/ddk -c -o $@ $<

build/basefs.bin: build/basefs.o
	x86_64-w64-mingw32-objcopy -O binary --only-section=.text $< $@

build/basefs_cases.o: basefs_cases.c | build
	$(CC) $(CFLAGS) -fmax-errors=3 -I/usr/x86_64-w64-mingw32/include/ddk -c -o $@ $<

build/basefs_test.exe: basefs_test.c build/basefs_cases.o
	$(CC) $(CFLAGS) -o $@ basefs_test.c build/basefs_cases.o

build/icu_test.exe: icu_test.c | build
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf build
