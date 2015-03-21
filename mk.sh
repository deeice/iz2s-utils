#!/bin/sh

libc_crt="/usr/local/musl/lib/crt1.o"
libc_start="/usr/local/musl/lib/crti.o"
libc_end="/usr/local/musl/lib/crtn.o"

libgcc="$(gcc -print-file-name=libgcc.a)"
libgcc=${libgcc%/libgcc.a}

gcc -static -Os -std=gnu99 -nostdinc -nostdlib \
  -I "/usr/local/musl/include" \
  "$libc_start" "$libc_crt" "$@" "$libc_end" \
  -Wl,--start-group -lc -lgcc -lgcc_eh -Wl,--end-group \
  -Wl,-nostdlib \
  -L /usr/local/musl/lib -L "$libgcc"

